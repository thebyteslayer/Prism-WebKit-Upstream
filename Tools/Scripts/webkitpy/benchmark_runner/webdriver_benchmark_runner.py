import json
import logging
import time
import os

from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner


_log = logging.getLogger(__name__)

class WebDriverBenchmarkRunner(BenchmarkRunner):
    name = 'webdriver'

    def _get_result(self, driver):
        result = driver.execute_script("return window.webdriver_results")
        return result

    def _run_one_test(self, web_root, test_file, iteration):
        from selenium.webdriver.support.ui import WebDriverWait
        try:
            url = 'file://{root}/{plan_name}/{test_file}{subtest_url}'.format(root=web_root, plan_name=self._plan_name, test_file=test_file, subtest_url=self._construct_subtest_url(self._subtests))
            driver = self._browser_driver.launch_driver(url, self._plan['options'], self._build_dir, self._browser_path)
            _log.info('Waiting on results from web browser')
            result = WebDriverWait(driver, self._plan['timeout'], poll_frequency=1.0).until(self._get_result)
            driver.quit()
        except Exception as error:
            self._browser_driver.diagnose_test_failure(self._diagnose_dir, error)
            raise error
        else:
            if not any(file.startswith('test-successful-screenshot-') for file in os.listdir(self._diagnose_dir)):
                self._browser_driver._save_screenshot_to_path(self._diagnose_dir, f'test-successful-screenshot-{int(time.time())}.jpg')
            self._browser_driver.close_browsers()

        return json.loads(result)
