"""Summarize PresentMon --v1_metrics CSV; no third-party dependencies required."""
import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from pathlib import Path


METRICS = (
    "MsBetweenPresents", "MsInPresentAPI", "MsBetweenDisplayChange",
    "MsUntilDisplayed", "MsUntilRenderComplete",
)


def number(value):
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (ValueError, TypeError):
        return None


def percentile(ordered, fraction):
    index = (len(ordered) - 1) * fraction
    low = int(index)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (index - low)


def distribution(values, refresh_hz):
    ordered = sorted(values)
    if not ordered:
        return {"samples": 0}
    result = {"samples": len(ordered), "mean_ms": sum(ordered) / len(ordered)}
    for label, fraction in (("p50_ms", .5), ("p95_ms", .95), ("p99_ms", .99), ("p99_9_ms", .999)):
        result[label] = percentile(ordered, fraction)
    result["max_ms"] = ordered[-1]
    for limit in (1, 5, 10, 20, 50):
        result[f"above_{limit}ms"] = sum(value > limit for value in ordered)
    if refresh_hz > 0:
        period = 1000 / refresh_hz
        result["above_1_5_refresh_periods"] = sum(value > period * 1.5 for value in ordered)
        result["above_2_5_refresh_periods"] = sum(value > period * 2.5 for value in ordered)
    return result


def analyze(path, refresh_hz=165.0, warmup_seconds=3.0):
    if refresh_hz <= 0 or warmup_seconds < 0:
        raise ValueError("Refresh rate must be positive and warmup must be nonnegative")
    groups = defaultdict(list)
    with Path(path).open(encoding="utf-8-sig", newline="") as source:
        reader = csv.DictReader(source)
        required = {"TimeInSeconds", "MsBetweenPresents", "ProcessID", "SwapChainAddress"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError("Expected PresentMon --v1_metrics CSV with TimeInSeconds and MsBetweenPresents")
        for row in reader:
            timestamp = number(row.get("TimeInSeconds"))
            if timestamp is not None:
                key = (row.get("Application", ""), row["ProcessID"], row["SwapChainAddress"])
                groups[key].append((timestamp, row))
    if not groups:
        raise ValueError("CSV has no timed frame records; no performance conclusion is possible")

    result = {"source": str(Path(path).resolve()), "refresh_hz_for_thresholds": refresh_hz,
              "warmup_seconds": warmup_seconds, "swapchains": [],
              "limitations": [
                  "Intervals are ETW presentation metrics, not instrumented input/simulation/render stages.",
                  "OpenGL may lack some metrics. Zero/NA display intervals are not proof of smooth display.",
                  "Refresh thresholds flag long intervals; they do not prove missed vblanks.",
                  "Focus changes, loading and menus can cause spikes; inspect the capture context.",
              ]}
    for key, records in groups.items():
        first = min(timestamp for timestamp, _ in records)
        selected = [(timestamp, row) for timestamp, row in records if timestamp - first >= warmup_seconds]
        values = {metric: [] for metric in METRICS}
        largest = []
        modes = Counter()
        runtimes = Counter()
        dropped = 0
        for timestamp, row in selected:
            is_dropped = row.get("Dropped", "").strip().lower() in ("1", "true")
            dropped += is_dropped
            modes[row.get("PresentMode", "unavailable")] += 1
            runtimes[row.get("Runtime", "unavailable")] += 1
            for metric in METRICS:
                value = number(row.get(metric))
                # A discarded present is not an additional display change.
                if metric == "MsBetweenDisplayChange" and is_dropped:
                    continue
                if value is not None and value >= 0 and (value > 0 or metric == "MsInPresentAPI"):
                    values[metric].append(value)
            interval = number(row.get("MsBetweenPresents"))
            if interval is not None and interval > 0:
                largest.append({"time_seconds": timestamp, "present_interval_ms": interval,
                                "present_api_ms": number(row.get("MsInPresentAPI")),
                                "display_interval_ms": number(row.get("MsBetweenDisplayChange")),
                                "dropped": is_dropped, "present_mode": row.get("PresentMode")})
        stats = {metric: distribution(samples, refresh_hz) for metric, samples in values.items()}
        present = stats["MsBetweenPresents"]
        result["swapchains"].append({
            "application": key[0], "process_id": key[1], "swapchain": key[2],
            "rows_total": len(records), "rows_analyzed": len(selected), "dropped_rows": dropped,
            "span_seconds": max(timestamp for timestamp, _ in selected) - min(timestamp for timestamp, _ in selected) if selected else 0,
            "present_modes": dict(modes), "runtimes": dict(runtimes), "metrics": stats,
            "average_present_fps": 1000 / present["mean_ms"] if present.get("mean_ms", 0) else None,
            "largest_present_intervals": sorted(largest, key=lambda item: item["present_interval_ms"], reverse=True)[:20],
        })
    result["swapchains"].sort(key=lambda group: group["rows_analyzed"], reverse=True)
    return result


def text_report(result):
    lines = [f"Source: {result['source']}", f"Warmup excluded per swapchain: {result['warmup_seconds']} s",
             f"Refresh used ONLY for thresholds: {result['refresh_hz_for_thresholds']} Hz"]
    for group in result["swapchains"]:
        lines += ["", f"{group['application']} PID={group['process_id']} swapchain={group['swapchain']}",
                  f"Rows analyzed={group['rows_analyzed']}, discarded presents={group['dropped_rows']}",
                  f"Presentation modes: {group['present_modes']}"]
        for name, stats in group["metrics"].items():
            if not stats["samples"]:
                lines.append(f"  {name}: unavailable (no usable samples)")
                continue
            lines.append(f"  {name}: n={stats['samples']} mean={stats['mean_ms']:.3f} ms "
                         f"p99={stats['p99_ms']:.3f} p99.9={stats['p99_9_ms']:.3f} max={stats['max_ms']:.3f}")
        lines.append("Largest present intervals (seconds from capture start):")
        for spike in group["largest_present_intervals"][:10]:
            lines.append(f"  t={spike['time_seconds']:.3f}s interval={spike['present_interval_ms']:.3f}ms "
                         f"API={spike['present_api_ms']}ms display={spike['display_interval_ms']}ms")
    lines += ["", "Limitations:"] + [f"- {item}" for item in result["limitations"]]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--refresh-hz", type=float, default=165)
    parser.add_argument("--warmup", type=float, default=3)
    args = parser.parse_args()
    try:
        result = analyze(args.csv, args.refresh_hz, args.warmup)
    except (ValueError, OSError) as error:
        parser.exit(1, f"Cannot analyze capture: {error}\n")
    report = text_report(result)
    args.csv.with_suffix(".summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    args.csv.with_suffix(".summary.txt").write_text(report, encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
