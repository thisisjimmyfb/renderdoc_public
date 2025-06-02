from typing import Tuple

import renderdoc as rd
import rdtest

class CoordExpectedColor:
    def __init__(self, coord : Tuple[float, float], color: Tuple[float, float, float, float]):
        self.x = coord[0]
        self.y = coord[1]
        self.color = color


class VK_Ray_Query(rdtest.TestCase):
    demos_test_name = 'VK_Ray_Query'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        out = last_action.copyDestination

        background_color = [0.1, 0.1, 0.1, 1.0]
        shadow_color = [0.6, 0.6, 0.6, 1.0]
        tri_color = [1.0, 1.0, 1.0, 1.0]

        coords = [
            # inside edge of triangle
            CoordExpectedColor([0.0, 0.285], tri_color),
            CoordExpectedColor([-0.285, -0.285], tri_color),
            CoordExpectedColor([0.285, -0.285], tri_color),

            # outside edge of triangle, in shadow
            CoordExpectedColor([0.0, 0.301], shadow_color),

            # outer edge of shadow
            CoordExpectedColor([0.0, 0.47], shadow_color),

            # below tri not in shadow
            CoordExpectedColor([0.0, 0.5], tri_color),

            # background coords outside of lower triangle
            CoordExpectedColor([0.0, -0.805], background_color),
            CoordExpectedColor([-0.805, 0.805], background_color),
            CoordExpectedColor([0.805, 0.805], background_color)
        ]

        tex_details = self.get_texture(out)
        x_max_new = tex_details.width
        y_max_new = tex_details.height

        for coord in coords:
            (new_x, new_y) = rdtest.util.transform_coord_from_vk_ndc(coord.x, coord.y, x_max_new, y_max_new)
            self.check_pixel_value(out, new_x , new_y, coord.color)
