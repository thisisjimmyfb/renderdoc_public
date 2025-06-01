import renderdoc as rd
import rdtest


class VK_Counters(rdtest.TestCase):
    demos_test_name = 'VK_Counters'

    def check_capture(self):
        avail = self.controller.EnumerateCounters()
        wanted = [rd.GPUCounter.EventGPUDuration, rd.GPUCounter.VSInvocations, rd.GPUCounter.PSInvocations,
                  rd.GPUCounter.IAPrimitives, rd.GPUCounter.RasterizedPrimitives, rd.GPUCounter.RasterizerInvocations,
                  rd.GPUCounter.SamplesPassed]

        counters = list(set(avail).intersection(set(wanted)))

        results = self.controller.FetchCounters(counters)
        descs = {}

        for c in counters:
            descs[c] = self.controller.DescribeCounter(c)

        action = self.find_action("Draw")
        self.controller.SetFrameEvent(action.eventId, False)
        durationAction = action.next

        # filter to only results from the draw
        results = [r for r in results if r.eventId == action.eventId or r.eventId == durationAction.eventId]

        ps = samp = None

        # The test triangle takes up about an eighth of the viewport
        pipe: rd.PipeState = self.controller.GetPipelineState()
        vp: rd.Viewport = pipe.GetViewport(0)
        sample_counts = 0.125 * vp.width * vp.height

        for r in results:
            desc: rd.CounterDescription = descs[r.counter]

            if r.eventId == durationAction.eventId:
                if r.counter == rd.GPUCounter.EventGPUDuration:
                    val = 0.0
                    if desc.resultByteWidth == 8:
                        val = r.value.d
                    elif desc.resultByteWidth == 4:
                        val = r.value.f

                    # should not be smaller than 0.1 microseconds, and should not be more than 50 milliseconds
                    if val < 1.0e-7 or val > 0.05:
                        raise rdtest.TestFailureException("{} of draw {}s is unexpected".format(desc.name, val))
                    else:
                        rdtest.log.success("{} of draw {}s is expected".format(desc.name, val))
            else:
                if (r.counter == rd.GPUCounter.IAPrimitives or r.counter == rd.GPUCounter.RasterizedPrimitives or
                      r.counter == rd.GPUCounter.RasterizerInvocations):
                    val = 0
                    if desc.resultByteWidth == 8:
                        val = r.value.u64
                    elif desc.resultByteWidth == 4:
                        val = r.value.u32

                    if val != 1:
                        raise rdtest.TestFailureException("{} of draw {} is unexpected".format(desc.name, val))
                    else:
                        rdtest.log.success("{} of draw {} is expected".format(desc.name, val))
                elif r.counter == rd.GPUCounter.VSInvocations:
                    val = 0
                    if desc.resultByteWidth == 8:
                        val = r.value.u64
                    elif desc.resultByteWidth == 4:
                        val = r.value.u32

                    if val != 3:
                        raise rdtest.TestFailureException("{} of draw {} is unexpected".format(desc.name, val))
                    else:
                        rdtest.log.success("{} of draw {} is expected".format(desc.name, val))
                elif r.counter == rd.GPUCounter.PSInvocations or r.counter == rd.GPUCounter.SamplesPassed:
                    val = 0
                    if desc.resultByteWidth == 8:
                        val = r.value.u64
                    elif desc.resultByteWidth == 4:
                        val = r.value.u32

                    if r.counter == rd.GPUCounter.PSInvocations:
                        ps = val
                    else:
                        samp = val

                    # Allow for slight rasterization differences with a 5% tolerance
                    if val < (sample_counts * 0.95) or val > (sample_counts * 1.05):
                        raise rdtest.TestFailureException("{} of draw {} is unexpected".format(desc.name, val))
                    else:
                        rdtest.log.success("{} of draw {} is expected".format(desc.name, val))

        if ps is not None and samp is not None:
            # allow 500 difference for overshading counting
            if abs(ps - samp) > 500:
                raise rdtest.TestFailureException("Samples passed {} and PS invocations {} don't match".format(samp, ps))
            else:
                rdtest.log.success("Samples passed {} and PS invocations {} match".format(samp, ps))

        rdtest.log.success("All counters have expected values")
