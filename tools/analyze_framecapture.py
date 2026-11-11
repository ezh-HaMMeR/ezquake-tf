"""Analyze the buffered CPU frame CSV produced by ezquake-diag executables."""
import argparse
from array import array
from collections import Counter
import csv
import heapq
import json
from pathlib import Path

from analyze_presentmon import distribution, number


def analyze(path, warmup=3.0, include_inactive=False):
    values = {key: array('d') for key in (
        'interval_ms', 'work_ms', 'input_ms', 'render_ms', 'swap_ms', 'tail_ms',
        'render_without_swap_ms', 'gap_before_frame_ms')}
    states = Counter()
    spikes = []
    previous = None
    total = selected = 0
    settings = Counter()
    marker_times = []
    last_marker = 0
    with Path(path).open(encoding='utf-8-sig', newline='') as source:
        reader = csv.DictReader(line for line in source if not line.startswith('#'))
        required = {'time_s', 'interval_ms', 'work_ms', 'input_ms', 'render_ms', 'swap_ms', 'focused', 'minimized', 'state', 'key_dest'}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError('Expected an ezquake framecapture CSV')
        for row in reader:
            total += 1
            row = {key: number(value) for key, value in row.items()}
            if any(row[key] is None for key in required):
                previous = None
                continue
            marker = row.get('marker') or 0
            if marker != last_marker:
                marker_times.append({'marker': marker, 'time_s': row['time_s']})
                last_marker = marker
            states[(row['focused'], row['minimized'], row['state'], row['key_dest'])] += 1
            eligible = row['time_s'] >= warmup and row['interval_ms'] > 0
            if not include_inactive:
                # ca_active=4, key_game=0. Require the preceding frame too: its work
                # and any focus/menu transition contribute to the current interval.
                for frame in (row, previous):
                    eligible = eligible and frame is not None and frame['focused'] == 1 and frame['minimized'] == 0 and frame['state'] == 4 and frame['key_dest'] == 0 and frame['time_s'] >= warmup
            if eligible:
                selected += 1
                for key in values:
                    value = row.get(key)
                    if key == 'render_without_swap_ms':
                        value = max(0, row['render_ms'] - row['swap_ms'])
                    elif key == 'gap_before_frame_ms':
                        value = max(0, row['interval_ms'] - previous['work_ms']) if previous else None
                    if value is not None and value >= 0:
                        # Zero is meaningful for a CPU stage with no work.
                        values[key].append(value)
                config = tuple(row.get(key) for key in ('vsync', 'lag_fix', 'tweak_ms', 'maxfps', 'hz'))
                settings[config] += 1
                # interval_ms ends at this frame's START, while its work metrics
                # describe work AFTER that timestamp. Preserve the preceding frame
                # so an outlier is not incorrectly blamed on the following render.
                detail = dict(row)
                detail['preceding_frame'] = dict(previous) if previous else None
                item = (row['interval_ms'], total, detail)
                if len(spikes) < 20:
                    heapq.heappush(spikes, item)
                elif item[:2] > spikes[0][:2]:
                    heapq.heapreplace(spikes, item)
            previous = row
    if not total:
        raise ValueError('CSV contains no frames')
    hz_values = [config[4] for config in settings if config[4] and config[4] > 0]
    hz = hz_values[0] if len(set(hz_values)) == 1 else 0
    return {
        'source': str(Path(path).resolve()), 'rows_total': total, 'rows_analyzed': selected,
        'warmup_seconds': warmup, 'include_inactive': include_inactive,
        'metrics': {key: distribution(data, hz) for key, data in values.items()},
        'settings': [dict(zip(('vsync', 'lag_fix', 'tweak_ms', 'maxfps', 'hz', 'frames'), (*config, count))) for config, count in settings.items()],
        'observed_states': [{'focused': state[0], 'minimized': state[1], 'connection_state': state[2], 'key_dest': state[3], 'frames': count} for state, count in states.items()],
        'markers': marker_times,
        'largest_intervals': [item[2] for item in sorted(spikes, reverse=True)],
        'limitations': [
            'CPU wall-clock timings, not GPU duration or physical display scanout.',
            'render_ms includes swap_ms; render_without_swap_ms subtracts that wait but can include driver stalls.',
            'gap_before_frame_ms includes frame limiting, VSync lag scheduling, OS scheduling and work outside CL_Frame.',
            'Default analysis excludes warmup, menus, unfocused/minimized frames and their immediate transitions.',
            'interval_ms is start-to-start; work fields belong to the following frame. Outliers include preceding_frame for attribution.',
            'A marker reflects reaction time after a stutter; inspect preceding frames as well.',
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    parser.add_argument('--warmup', type=float, default=3)
    parser.add_argument('--include-inactive', action='store_true')
    args = parser.parse_args()
    if args.warmup < 0:
        parser.error('Warmup must be nonnegative')
    try:
        result = analyze(args.csv, args.warmup, args.include_inactive)
    except (OSError, ValueError) as error:
        parser.exit(1, f'Cannot analyze capture: {error}\n')
    output = args.csv.with_suffix('.summary.json')
    output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f"{result['rows_analyzed']} / {result['rows_total']} frames analyzed; summary: {output}")
    if not result['rows_analyzed']:
        parser.exit(1, 'No eligible gameplay frames; no performance conclusion is possible.\n')
    for key, stats in result['metrics'].items():
        if stats['samples']:
            print(f"{key}: mean={stats['mean_ms']:.3f} p99={stats['p99_ms']:.3f} p99.9={stats['p99_9_ms']:.3f} max={stats['max_ms']:.3f}")


if __name__ == '__main__':
    main()
