import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("analyze_presentmon", Path(__file__).resolve().parents[1] / "tools/analyze_presentmon.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PresentMonAnalysisTests(unittest.TestCase):
    def analyze(self, body, warmup=0):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "capture.csv"
            path.write_text(body, encoding="utf-8-sig")
            return module.analyze(path, 100, warmup)

    def test_warmup_swapchains_drops_and_unavailable_metrics(self):
        result = self.analyze(
            'Application,ProcessID,SwapChainAddress,TimeInSeconds,MsBetweenPresents,MsInPresentAPI,MsBetweenDisplayChange,Dropped,PresentMode\n'
            'ezquake.exe,1,a,0,100,0,100,0,"Hardware: Legacy Flip"\n'
            'ezquake.exe,1,a,4,10,0,10,0,"Hardware: Legacy Flip"\n'
            'ezquake.exe,1,a,5,30,5,999,1,"Hardware: Legacy Flip"\n'
            'ezquake.exe,1,a,6,20,NA,0,0,"Hardware: Legacy Flip"\n'
            'ezquake.exe,1,b,0,500,0,0,0,Other\n'
            'ezquake.exe,1,b,4,2,0,0,0,Other\n', warmup=3)
        primary, secondary = result['swapchains']
        self.assertEqual(primary['rows_analyzed'], 3)
        self.assertEqual(primary['metrics']['MsBetweenPresents']['mean_ms'], 20)
        self.assertEqual(primary['metrics']['MsBetweenPresents']['p99_ms'], 29.8)
        self.assertEqual(primary['metrics']['MsBetweenDisplayChange']['samples'], 1)
        self.assertEqual(primary['metrics']['MsInPresentAPI']['samples'], 2)
        self.assertEqual(primary['metrics']['MsUntilDisplayed']['samples'], 0)
        self.assertEqual(primary['dropped_rows'], 1)
        self.assertEqual(primary['largest_present_intervals'][0]['time_seconds'], 5)
        self.assertEqual(secondary['metrics']['MsBetweenPresents']['mean_ms'], 2)
        self.assertIn('unavailable', module.text_report(result))

    def test_no_rows_or_wrong_format_does_not_report_success(self):
        for body in ('TimeInSeconds,MsBetweenPresents,ProcessID,SwapChainAddress\n', 'CPUStartTime,MsCPUBusy\n1,2\n'):
            with self.assertRaises(ValueError):
                self.analyze(body)

    def test_nonfinite_numbers_are_missing(self):
        self.assertIsNone(module.number('NaN'))
        self.assertIsNone(module.number('inf'))
        self.assertIsNone(module.number('NA'))
        self.assertEqual(module.number('0'), 0)


if __name__ == '__main__':
    unittest.main()
