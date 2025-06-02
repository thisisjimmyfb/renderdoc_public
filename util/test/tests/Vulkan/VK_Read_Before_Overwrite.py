import renderdoc as rd
import rdtest


class VK_Read_Before_Overwrite(rdtest.TestCase):
    demos_test_name = 'VK_Read_Before_Overwrite'

    def check_capture(self):
        # force a few replays to ensure we are definitely in a stable point - the first replay after load may have
        # slightly different contents (in the case of bugs)
        self.controller.SetFrameEvent(self.find_action("checkpoint").eventId, True)
        self.controller.SetFrameEvent(self.find_action("checkpoint").eventId, True)
        self.controller.SetFrameEvent(self.find_action("checkpoint").eventId, True)

        vp = self.screen_crop_coords()
        self.check_triangle(None, [0.2, 0.2, 0.2, 1.0], [1.0, 1.0, 1.0, 1.0], (0.0, 0.0, vp[2]*0.5, vp[3]))
        self.check_triangle(None, [0.2, 0.2, 0.2, 1.0], [1.0, 1.0, 1.0, 1.0], (vp[2]*0.5, 0.0, vp[2]*0.5, vp[3]))

        rdtest.log.success('Triangle is rendered correctly')
