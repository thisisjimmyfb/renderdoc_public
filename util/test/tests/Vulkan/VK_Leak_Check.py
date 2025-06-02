import renderdoc as rd
import rdtest


class VK_Leak_Check(rdtest.TestCase):
    demos_test_name = 'VK_Leak_Check'
    demos_frame_cap = 50000
    demos_frame_count = 10
    demos_timeout = 120

    def __init__(self):
        if not rdtest.util.get_remote_server() is None:
            # Reduce from the original 50000 as Android displays are typically V-synced at 60fps,
            # so 50000 frames is ~15mins - we're not waiting that long.  But we don't need to as
            # on Android the backbuffer is fullscreen so leaks affect consumption much faster than
            # other platforms
            self.demos_frame_cap = 5000

    def check_capture(self):
        memory: int = rd.GetCurrentProcessMemoryUsage()

        if memory > 500*1000*1000:
            raise rdtest.TestFailureException("Memory usage of {} is too high".format(memory))

        rdtest.log.success("Capture {} opened with reasonable memory ({})".format(self.demos_frame_cap, memory))
