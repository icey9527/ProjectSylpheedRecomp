#include <rex/runtime.h>
#include <rex/kernel/init.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) try {
  if (argc != 3) { std::cerr << "dump_xex_image INPUT_XEX OUTPUT_DIRECTORY\n"; return 2; }
  const auto input = std::filesystem::absolute(argv[1]);
  const auto output = std::filesystem::absolute(argv[2]);
  if (!std::filesystem::is_regular_file(input)) return 3;
  const auto relative = std::filesystem::weakly_canonical(output).lexically_relative(
      std::filesystem::canonical(input.parent_path()));
  if (!relative.empty() && *relative.begin() != "..") {
    std::cerr << "Keep inspection output outside the input resource directory\n";
    return 8;
  }
  std::filesystem::create_directories(output);
  rex::Runtime runtime(input.parent_path(), output / "tool-user-data");
  if (runtime.Setup(rex::RuntimeConfig{.kernel_init = rex::kernel::InitializeKernel,
                                      .tool_mode = true}) != 0) return 4;
  if (runtime.LoadXexImage("game:\\" + input.filename().string()) != 0) return 5;
  const auto module = runtime.kernel_state()->GetExecutableModule();
  if (!module || !module->xex_module()) return 5;
  const auto* xex = module->xex_module();
  std::ofstream image(output / "image.bin", std::ios::binary);
  image.write(reinterpret_cast<const char*>(runtime.virtual_membase() + xex->base_address()), xex->image_size());
  if (!image) return 6;
  std::ofstream meta(output / "sections.tsv");
  meta << "base\t" << std::hex << xex->base_address() << "\nsize\t" << xex->image_size() << "\n";
  for (const auto& s : xex->binary_sections())
    meta << s.name << '\t' << std::hex << s.virtual_address << '\t' << s.virtual_size << '\t' << s.executable << '\n';
  return meta ? 0 : 7;
} catch (const std::exception& error) {
  std::cerr << "Image inspection failed: " << error.what() << '\n';
  return 1;
}
