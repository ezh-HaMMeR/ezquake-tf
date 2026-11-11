import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import analyze_framecapture


class FrameCaptureAnalysisTests(unittest.TestCase):
    def test_excludes_focus_transitions_and_separates_render_wait(self):
        header = '# diagnostic\ntime_s,interval_ms,work_ms,input_ms,render_ms,swap_ms,tail_ms,focused,minimized,state,key_dest,marker,vsync,lag_fix,tweak_ms,maxfps,hz\n'
        data = (
            '0,0,2,.1,1,.5,.1,1,0,4,0,0,1,1,1,0,100\n'
            '4,10,3,.1,2,1,.1,0,0,4,0,0,1,1,1,0,100\n'
            '5,100,3,.1,2,1,.1,1,0,4,0,0,1,1,1,0,100\n'
            '6,10,4,.1,3,2,.1,1,0,4,0,1,1,1,1,0,100\n'
            '7,20,5,.1,4,3,.1,1,0,4,0,1,1,1,1,0,100\n')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frames.csv'
            path.write_text(header + data, encoding='utf-8')
            result = analyze_framecapture.analyze(path)
        self.assertEqual(result['rows_total'], 5)
        self.assertEqual(result['rows_analyzed'], 2)
        self.assertEqual(result['metrics']['interval_ms']['mean_ms'], 15)
        self.assertEqual(result['metrics']['render_without_swap_ms']['mean_ms'], 1)
        self.assertEqual(result['metrics']['gap_before_frame_ms']['mean_ms'], 11.5)
        self.assertEqual(result['markers'], [{'marker': 1, 'time_s': 6}])
        self.assertEqual(result['largest_intervals'][0]['time_s'], 7)
        self.assertEqual(result['largest_intervals'][0]['preceding_frame']['time_s'], 6)


if __name__ == '__main__':
    unittest.main()
