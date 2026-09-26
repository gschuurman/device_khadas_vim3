// Headless panvk smoke test: compute, offscreen render + readback, AHB render + CPU readback.
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/hardware_buffer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static const uint32_t comp_spv[] = {
#include "comp_comp.inc"
};
static const uint32_t vert_spv[] = {
#include "tri_vert.inc"
};
static const uint32_t frag_spv[] = {
#include "tri_frag.inc"
};

#define CK(x)                                                              \
  do {                                                                     \
    VkResult r_ = (x);                                                     \
    if (r_ != VK_SUCCESS) {                                                \
      fprintf(stderr, "FAIL %s:%d %s = %d\n", __FILE__, __LINE__, #x, r_); \
      exit(1);                                                             \
    }                                                                      \
  } while (0)

#define W 256
#define H 256

static VkPhysicalDevice pd;
static VkDevice dev;
static VkQueue q;
static uint32_t qfam;
static VkCommandPool pool;
static VkPhysicalDeviceMemoryProperties memprops;
static int fails;

static uint32_t memtype(uint32_t bits, VkMemoryPropertyFlags want) {
  for (uint32_t i = 0; i < memprops.memoryTypeCount; i++)
    if ((bits & (1u << i)) && (memprops.memoryTypes[i].propertyFlags & want) == want) return i;
  fprintf(stderr, "no memtype bits=%x want=%x\n", bits, want);
  exit(1);
}

static void mkbuf(VkDeviceSize sz, VkBufferUsageFlags u, VkBuffer *b, VkDeviceMemory *m, void **map) {
  VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sz, .usage = u};
  CK(vkCreateBuffer(dev, &bi, NULL, b));
  VkMemoryRequirements mr;
  vkGetBufferMemoryRequirements(dev, *b, &mr);
  VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
      .memoryTypeIndex = memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
  CK(vkAllocateMemory(dev, &ai, NULL, m));
  CK(vkBindBufferMemory(dev, *b, *m, 0));
  CK(vkMapMemory(dev, *m, 0, VK_WHOLE_SIZE, 0, map));
}

static VkShaderModule shader(const uint32_t *code, size_t sz) {
  VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sz, .pCode = code};
  VkShaderModule m;
  CK(vkCreateShaderModule(dev, &ci, NULL, &m));
  return m;
}

static VkCommandBuffer begin(void) {
  VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
  VkCommandBuffer cb;
  CK(vkAllocateCommandBuffers(dev, &ai, &cb));
  VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  CK(vkBeginCommandBuffer(cb, &bi));
  return cb;
}

static void submit(VkCommandBuffer cb) {
  CK(vkEndCommandBuffer(cb));
  VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence f;
  CK(vkCreateFence(dev, &fi, NULL, &f));
  VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
  CK(vkQueueSubmit(q, 1, &si, f));
  CK(vkWaitForFences(dev, 1, &f, VK_TRUE, 5000000000ull));
  vkDestroyFence(dev, f, NULL);
  vkFreeCommandBuffers(dev, pool, 1, &cb);
}

static void check(const char *what, int ok) {
  printf("%-40s %s\n", what, ok ? "PASS" : "FAIL");
  if (!ok) fails++;
}

static void test_compute(void) {
  const uint32_t n = 4096;
  VkBuffer b; VkDeviceMemory m; uint32_t *p;
  mkbuf(n * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &b, &m, (void **)&p);
  memset(p, 0, n * 4);
  VkDescriptorSetLayoutBinding lb = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
  VkDescriptorSetLayoutCreateInfo lci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &lb};
  VkDescriptorSetLayout dsl; CK(vkCreateDescriptorSetLayout(dev, &lci, NULL, &dsl));
  VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl};
  VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkComputePipelineCreateInfo cpi = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader(comp_spv, sizeof(comp_spv)), .pName = "main"},
      .layout = pl};
  VkPipeline pipe; CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpi, NULL, &pipe));
  VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
  VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &ps};
  VkDescriptorPool dp; CK(vkCreateDescriptorPool(dev, &dpci, NULL, &dp));
  VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = dp, .descriptorSetCount = 1, .pSetLayouts = &dsl};
  VkDescriptorSet ds; CK(vkAllocateDescriptorSets(dev, &dai, &ds));
  VkDescriptorBufferInfo dbi = {b, 0, VK_WHOLE_SIZE};
  VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = ds, .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dbi};
  vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);
  VkCommandBuffer cb = begin();
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
  vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds, 0, NULL);
  vkCmdDispatch(cb, n / 64, 1, 1);
  VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
  submit(cb);
  int bad = 0;
  for (uint32_t i = 0; i < n; i++) if (p[i] != i * 3 + 7) { if (bad++ < 3) fprintf(stderr, "  v[%u]=%u\n", i, p[i]); }
  check("compute: 4096 invocations", bad == 0);
}

static VkRenderPass rp;
static VkPipeline gpipe;

static void setup_graphics(void) {
  VkAttachmentDescription ad = {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
      VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
  VkAttachmentReference ar = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sd = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &ar};
  VkRenderPassCreateInfo rci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = 1, .pAttachments = &ad,
      .subpassCount = 1, .pSubpasses = &sd};
  CK(vkCreateRenderPass(dev, &rci, NULL, &rp));
  VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  VkPipelineLayout pl; CK(vkCreatePipelineLayout(dev, &plci, NULL, &pl));
  VkPipelineShaderStageCreateInfo st[2] = {
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = shader(vert_spv, sizeof(vert_spv)), .pName = "main"},
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = shader(frag_spv, sizeof(frag_spv)), .pName = "main"}};
  VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkViewport vp = {0, 0, W, H, 0, 1};
  VkRect2D sc = {{0, 0}, {W, H}};
  VkPipelineViewportStateCreateInfo vs = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .pViewports = &vp, .scissorCount = 1, .pScissors = &sc};
  VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .lineWidth = 1};
  VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineColorBlendAttachmentState cba = {.colorWriteMask = 0xf};
  VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &cba};
  VkGraphicsPipelineCreateInfo gci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .stageCount = 2, .pStages = st,
      .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vs, .pRasterizationState = &rs,
      .pMultisampleState = &ms, .pColorBlendState = &cb, .layout = pl, .renderPass = rp};
  CK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gci, NULL, &gpipe));
}

// Clears img to red, draws a green triangle; if dst != NULL copies the result into it.
static void draw(VkImage img, VkBuffer dst, int release_foreign) {
  VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = img, .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  VkImageView iv; CK(vkCreateImageView(dev, &vci, NULL, &iv));
  VkFramebufferCreateInfo fci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = rp, .attachmentCount = 1,
      .pAttachments = &iv, .width = W, .height = H, .layers = 1};
  VkFramebuffer fb; CK(vkCreateFramebuffer(dev, &fci, NULL, &fb));
  VkCommandBuffer cb = begin();
  VkClearValue cv = {.color = {{1, 0, 0, 1}}};
  VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = rp, .framebuffer = fb,
      .renderArea = {{0, 0}, {W, H}}, .clearValueCount = 1, .pClearValues = &cv};
  vkCmdBeginRenderPass(cb, &rbi, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gpipe);
  vkCmdDraw(cb, 3, 1, 0, 0);
  vkCmdEndRenderPass(cb);
  if (dst) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT, .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = img, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
    VkBufferImageCopy r = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {W, H, 1}};
    vkCmdCopyImageToBuffer(cb, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &r);
    VkMemoryBarrier mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
  }
  if (release_foreign) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = qfam, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT, .image = img,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &b);
  }
  submit(cb);
  vkDestroyFramebuffer(dev, fb, NULL);
  vkDestroyImageView(dev, iv, NULL);
}

// Expects red outside, green in the triangle.
static int check_pixels(const uint8_t *p, uint32_t stride_bytes, const char *tag) {
  const uint8_t *in = p + (H * 6 / 10) * stride_bytes + (W / 2) * 4;
  const uint8_t *out = p + 2 * stride_bytes + 2 * 4;
  printf("  %s: inside=%02x%02x%02x%02x corner=%02x%02x%02x%02x\n", tag, in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3]);
  return in[0] == 0 && in[1] == 0xff && in[2] == 0 && out[0] == 0xff && out[1] == 0 && out[2] == 0;
}

static void test_render(void) {
  VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = {W, H, 1}, .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
  VkImage img; CK(vkCreateImage(dev, &ici, NULL, &img));
  VkMemoryRequirements mr; vkGetImageMemoryRequirements(dev, img, &mr);
  VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size,
      .memoryTypeIndex = memtype(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
  VkDeviceMemory m; CK(vkAllocateMemory(dev, &ai, NULL, &m));
  CK(vkBindImageMemory(dev, img, m, 0));
  VkBuffer b; VkDeviceMemory bm; uint8_t *p;
  mkbuf(W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &b, &bm, (void **)&p);
  draw(img, b, 0);
  check("render: triangle + copy to buffer", check_pixels(p, W * 4, "offscreen"));
}

static void test_ahb(void) {
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID getprops =
      (void *)vkGetDeviceProcAddr(dev, "vkGetAndroidHardwareBufferPropertiesANDROID");
  if (!getprops) { check("ahb: extension entrypoint", 0); return; }
  AHardwareBuffer_Desc d = {.width = W, .height = H, .layers = 1, .format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
      .usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN};
  AHardwareBuffer *ahb;
  if (AHardwareBuffer_allocate(&d, &ahb)) { check("ahb: allocate", 0); return; }
  AHardwareBuffer_describe(ahb, &d);
  VkAndroidHardwareBufferFormatPropertiesANDROID fp = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID};
  VkAndroidHardwareBufferPropertiesANDROID ap = {VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID, .pNext = &fp};
  CK(getprops(dev, ahb, &ap));
  printf("  ahb: stride=%u size=%llu memTypeBits=%x format=%d\n", d.stride, (unsigned long long)ap.allocationSize, ap.memoryTypeBits, fp.format);
  VkExternalMemoryImageCreateInfo emi = {VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID};
  VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = &emi, .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM, .extent = {W, H, 1}, .mipLevels = 1, .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT};
  VkImage img; CK(vkCreateImage(dev, &ici, NULL, &img));
  VkMemoryDedicatedAllocateInfo ded = {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .image = img};
  VkImportAndroidHardwareBufferInfoANDROID imp = {VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID, .pNext = &ded, .buffer = ahb};
  VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = &imp, .allocationSize = ap.allocationSize,
      .memoryTypeIndex = memtype(ap.memoryTypeBits, 0)};
  VkDeviceMemory m; CK(vkAllocateMemory(dev, &ai, NULL, &m));
  CK(vkBindImageMemory(dev, img, m, 0));
  draw(img, VK_NULL_HANDLE, 1);
  void *p = NULL;
  if (AHardwareBuffer_lock(ahb, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, NULL, &p) || !p) { check("ahb: CPU lock", 0); return; }
  check("ahb: render into imported AHB, CPU read", check_pixels(p, d.stride * 4, "ahb"));
  AHardwareBuffer_unlock(ahb, NULL);
  vkDestroyImage(dev, img, NULL);
  vkFreeMemory(dev, m, NULL);
  AHardwareBuffer_release(ahb);
}

static int has_ext(VkExtensionProperties *e, uint32_t n, const char *name) {
  for (uint32_t i = 0; i < n; i++) if (!strcmp(e[i].extensionName, name)) return 1;
  return 0;
}

int main(void) {
  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "vktest", .apiVersion = VK_API_VERSION_1_0};
  const char *iext[] = {"VK_KHR_get_physical_device_properties2", "VK_KHR_external_memory_capabilities"};
  VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app, .enabledExtensionCount = 2, .ppEnabledExtensionNames = iext};
  VkInstance inst; CK(vkCreateInstance(&ici, NULL, &inst));
  uint32_t n = 1;
  VkResult r = vkEnumeratePhysicalDevices(inst, &n, &pd);
  if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || n == 0) { printf("no Vulkan device (r=%d n=%u)\n", r, n); return 1; }
  VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(pd, &pp);
  printf("device: %s api %u.%u.%u driver 0x%x\n", pp.deviceName, VK_API_VERSION_MAJOR(pp.apiVersion),
         VK_API_VERSION_MINOR(pp.apiVersion), VK_API_VERSION_PATCH(pp.apiVersion), pp.driverVersion);
  vkGetPhysicalDeviceMemoryProperties(pd, &memprops);
  uint32_t nq = 8; VkQueueFamilyProperties qp[8];
  vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qp);
  for (qfam = 0; qfam < nq; qfam++) if ((qp[qfam].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) break;
  if (qfam == nq) { printf("no gfx+compute queue\n"); return 1; }

  uint32_t ne = 0; vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, NULL);
  VkExtensionProperties *de = calloc(ne, sizeof(*de)); vkEnumerateDeviceExtensionProperties(pd, NULL, &ne, de);
  const char *want[] = {"VK_ANDROID_external_memory_android_hardware_buffer", "VK_KHR_sampler_ycbcr_conversion",
      "VK_KHR_external_memory", "VK_KHR_dedicated_allocation", "VK_KHR_get_memory_requirements2", "VK_KHR_bind_memory2",
      "VK_KHR_maintenance1", "VK_EXT_queue_family_foreign"};
  const char *en[8]; uint32_t nen = 0; int ahb_ok = 1;
  for (int i = 0; i < 8; i++) {
    if (has_ext(de, ne, want[i])) en[nen++] = want[i];
    else { printf("  missing %s\n", want[i]); ahb_ok = 0; }
  }
  float prio = 1;
  VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = qfam, .queueCount = 1, .pQueuePriorities = &prio};
  VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
      .enabledExtensionCount = nen, .ppEnabledExtensionNames = en};
  CK(vkCreateDevice(pd, &dci, NULL, &dev));
  vkGetDeviceQueue(dev, qfam, 0, &q);
  VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = qfam};
  CK(vkCreateCommandPool(dev, &pci, NULL, &pool));

  test_compute();
  setup_graphics();
  test_render();
  if (ahb_ok) test_ahb(); else check("ahb: required extensions", 0);
  CK(vkDeviceWaitIdle(dev));
  printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
  return fails != 0;
}
