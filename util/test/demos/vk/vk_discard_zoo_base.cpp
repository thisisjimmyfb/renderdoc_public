/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

struct VkDiscardZoo : VulkanGraphicsTest
{
  static constexpr const char *Description =
      "Tests the different discard patterns possible on replay.";

  const std::string pixel = R"EOSHADER(
#version 460 core

layout(location = 0, index = 0) out vec4 Color;

layout(set = 0, binding = 0, std140) uniform constsbuf
{
  vec4 value;
};

void main()
{
	Color = vec4(1.0f, 0.0f, 0.0f, 1.0f);

  if(value.y == 234.0f)
    Color = vec4(0.0f, 1.0f, 0.0f, 1.0f);
}

)EOSHADER";

  AllocatedBuffer emptyBuf;

  const VmaAllocationCreateInfo memGpuOnly = VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_GPU_ONLY});

  void PrepareTest(int argc, char **argv)
  {
    optDevExts.push_back(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR separateDepthStencilFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES_KHR,
    };

    if(std::find(devExts.begin(), devExts.end(),
                 VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME) != devExts.end())
    {
      getPhysFeatures2(&separateDepthStencilFeatures);

      if(!separateDepthStencilFeatures.separateDepthStencilLayouts)
        Avail = "'separateDepthStencilLayouts' not available";

      devInfoNext = &separateDepthStencilFeatures;
    }
  }

  void Clear(VkCommandBuffer cmd, const AllocatedImage &img)
  {
    if(img.image == VK_NULL_HANDLE)
      return;

    vkh::ImageSubresourceRange range;

    if(img.createInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
       img.createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(img.createInfo.format == VK_FORMAT_D32_SFLOAT ||
            img.createInfo.format == VK_FORMAT_D16_UNORM)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(img.createInfo.format == VK_FORMAT_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT |
                                             VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_GENERAL, img.image, range),
             });

    VkClearDepthStencilValue val = {0.4f, 0x40};

    if(img.createInfo.format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC2_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC3_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC4_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC5_UNORM_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC6H_UFLOAT_BLOCK ||
       img.createInfo.format == VK_FORMAT_BC7_UNORM_BLOCK)
    {
      // can't clear compressed formats with vkCmdClearColorImage
      VkBufferImageCopy region = {};
      std::vector<VkBufferImageCopy> regions;

      region.imageSubresource.aspectMask = range.aspectMask;
      region.imageSubresource.layerCount = img.createInfo.arrayLayers;

      for(uint32_t m = 0; m < img.createInfo.mipLevels; m++)
      {
        region.imageExtent.width = std::max(1U, img.createInfo.extent.width >> m);
        region.imageExtent.height = std::max(1U, img.createInfo.extent.height >> m);
        region.imageExtent.depth = std::max(1U, img.createInfo.extent.depth >> m);
        region.imageSubresource.mipLevel = m;

        regions.push_back(region);
      }

      vkCmdCopyBufferToImage(cmd, emptyBuf.buffer, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             (uint32_t)regions.size(), regions.data());
      return;
    }

    if(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
    {
      if(img.createInfo.format == VK_FORMAT_A2B10G10R10_UINT_PACK32)
        vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0u, 1023u, 0u, 1u), 1, range);
      else
        vkCmdClearColorImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL,
                             vkh::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f), 1, range);
    }
    else
    {
      vkCmdClearDepthStencilImage(cmd, img.image, VK_IMAGE_LAYOUT_GENERAL, &val, 1, range);
    }
  }

  void DiscardImage(VkCommandBuffer cmd, AllocatedImage &img, vkh::ImageSubresourceRange range = {})
  {
    if(img.image == VK_NULL_HANDLE)
    {
      return;
    }
    if(img.createInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
       img.createInfo.format == VK_FORMAT_D24_UNORM_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(img.createInfo.format == VK_FORMAT_D32_SFLOAT ||
            img.createInfo.format == VK_FORMAT_D16_UNORM)
      range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else if(img.createInfo.format == VK_FORMAT_S8_UINT)
      range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

    vkh::cmdPipelineBarrier(
        cmd, {
                 vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                         img.image, range),
             });
  }

  AllocatedImage MakeTex2D(VkFormat fmt, uint32_t width, uint32_t height, uint32_t mips = 1,
                           uint32_t arraySlices = 1)
  {
    VkImageCreateInfo imageInfo = vkh::ImageCreateInfo(
        width, height, 0, fmt, VK_IMAGE_USAGE_TRANSFER_DST_BIT, mips, arraySlices);

    if(!IsImageFormatSupported(imageInfo))
    {
      return AllocatedImage();
    }
    return AllocatedImage(this, imageInfo, memGpuOnly);
  }

  AllocatedImage MakeTex2DMS(VkFormat fmt, uint32_t width, uint32_t height, uint32_t samples,
                             uint32_t arraySlices = 1)
  {
    bool depth = (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D32_SFLOAT ||
                  fmt == VK_FORMAT_D24_UNORM_S8_UINT || fmt == VK_FORMAT_D16_UNORM ||
                  fmt == VK_FORMAT_S8_UINT);

    VkImageCreateInfo imageInfo = vkh::ImageCreateInfo(
        width, height, 0, fmt,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                 : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
        1, arraySlices, (VkSampleCountFlagBits)samples);

    if(!IsImageFormatSupported(imageInfo))
    {
      return AllocatedImage();
    }
    return AllocatedImage(this, imageInfo, memGpuOnly);
  }

  bool IsImageFormatSupported(const VkImageCreateInfo &imageInfo)
  {
    VkImageFormatProperties imageProperties;
    VkResult ret = vkGetPhysicalDeviceImageFormatProperties(
        phys, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage,
        imageInfo.flags, &imageProperties);

    if(ret == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
      return false;
    }
    if(ret != VK_SUCCESS)
    {
      return false;
    }
    if(imageProperties.maxArrayLayers < imageInfo.arrayLayers)
    {
      return false;
    }
    if(imageProperties.maxMipLevels < imageInfo.mipLevels)
    {
      return false;
    }
    if((imageInfo.samples > 1) && (features.shaderStorageImageMultisample != VK_TRUE))
    {
      return false;
    }
    if((static_cast<VkSampleCountFlagBits>(imageProperties.sampleCounts) & imageInfo.samples) !=
       imageInfo.samples)
    {
      return false;
    }
    if(imageProperties.maxExtent.depth < imageInfo.extent.depth)
    {
      return false;
    }
    if(imageProperties.maxExtent.width < imageInfo.extent.width)
    {
      return false;
    }
    if(imageProperties.maxExtent.height < imageInfo.extent.height)
    {
      return false;
    }

    return true;
  }
};
