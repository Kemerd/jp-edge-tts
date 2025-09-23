# JP Edge TTS 🎌 🔊

A high-performance, cross-platform Japanese Text-to-Speech (TTS) engine using ONNX Runtime and DeepPhonemizer. Built for edge deployment with C++ core and FFI-compatible C API for language bindings.

## ✨ Features

- **🚀 High Performance**: Multi-threaded architecture with ONNX Runtime optimization
- **🎌 Japanese-Optimized**: Native Japanese text processing with MeCab integration
- **🔧 Cross-Platform**: Windows, macOS, Linux support with platform-specific optimizations
- **📦 Multiple Interfaces**:
  - C++ API for native integration
  - C API for FFI bindings (Python, Rust, Go, etc.)
  - Command-line interface for immediate use
  - DLL/shared library support
- **🧠 AI-Powered**: DeepPhonemizer for accurate grapheme-to-phoneme conversion
- **🎯 Production-Ready**: Intelligent caching, queue management, and error handling
- **🎨 Multiple Voices**: Support for various Japanese voice styles

## 🏗️ Architecture

```
Japanese Text → Segmentation → Phonemization → Tokenization → ONNX Inference → Audio Output
     ↓              ↓              ↓              ↓               ↓              ↓
  [MeCab]    [DeepPhonemizer] [Dictionary]  [Vocabulary]   [Kokoro Model]   [WAV/PCM]
```

## 📋 Prerequisites

### Required
- CMake 3.18+
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Git (for submodules)

### Optional
- CUDA Toolkit (for GPU acceleration)
- MeCab (for Japanese text segmentation)
- Python 3.7+ (for training scripts)

## 🚀 Quick Start

### Clone and Initialize
```bash
git clone https://github.com/yourusername/jp-edge-tts.git
cd jp-edge-tts
git submodule update --init --recursive
```

### Build

#### Windows
```batch
# Using Visual Studio
build.bat

# With specific options
build.bat --enable-gpu --arch x64 --generator "Visual Studio 17 2022"

# Using MinGW
build.bat --generator "MinGW Makefiles"
```

#### macOS/Linux
```bash
# Default build
./build.sh

# With options
./build.sh --enable-gpu --jobs 8

# Install dependencies (Ubuntu/Debian)
./build.sh --install-deps
```

### Run CLI Application
```bash
# Simple synthesis
jp_tts "こんにちは、世界！" --output greeting.wav

# With voice selection
jp_tts "ゆっくり話します" --voice jf_alpha --speed 0.8

# Interactive mode
jp_tts --interactive

# Process JSON request
jp_tts --json --file request.json

# List available voices
jp_tts --list-voices
```

## 📖 Usage Examples

### C++ API
```cpp
#include <jp_edge_tts/core/tts_engine.h>

int main() {
    // Create and initialize engine
    auto engine = jp_edge_tts::CreateTTSEngine();
    engine->Initialize();

    // Simple synthesis
    auto result = engine->SynthesizeSimple("こんにちは", "jf_alpha");

    // Save to file
    if (result.IsSuccess()) {
        engine->SaveAudioToFile(result.audio, "output.wav");
    }

    return 0;
}
```

### C API (FFI)
```c
#include <jp_edge_tts/jp_edge_tts_c_api.h>

int main() {
    // Create engine
    jp_tts_engine_t engine = jp_tts_create_engine(NULL);
    jp_tts_initialize(engine);

    // Synthesize
    jp_tts_result_t result = jp_tts_synthesize_simple(
        engine,
        "こんにちは",
        "jf_alpha"
    );

    // Save audio
    jp_tts_result_save_to_file(result, "output.wav", JP_TTS_FORMAT_WAV_PCM16);

    // Cleanup
    jp_tts_result_free(result);
    jp_tts_destroy_engine(engine);

    return 0;
}
```

### Python Bindings (using ctypes)
```python
import ctypes
from ctypes import c_void_p, c_char_p

# Load the library
lib = ctypes.CDLL('./build/lib/jp_edge_tts_core.dll')  # Windows
# lib = ctypes.CDLL('./build/lib/libjp_edge_tts_core.so')  # Linux

# Define function signatures
lib.jp_tts_create_engine.restype = c_void_p
lib.jp_tts_synthesize_simple.restype = c_void_p

# Create engine
engine = lib.jp_tts_create_engine(None)
lib.jp_tts_initialize(engine)

# Synthesize
text = "こんにちは".encode('utf-8')
voice = "jf_alpha".encode('utf-8')
result = lib.jp_tts_synthesize_simple(engine, text, voice)

# Save to file
output = "output.wav".encode('utf-8')
lib.jp_tts_result_save_to_file(result, output, 0)  # 0 = WAV_PCM16

# Cleanup
lib.jp_tts_result_free(result)
lib.jp_tts_destroy_engine(engine)
```

### JSON Request Format
```json
{
    "text": "今日はいい天気ですね",
    "voice_id": "jf_alpha",
    "speed": 1.0,
    "pitch": 1.0,
    "volume": 0.9,
    "phonemes": "k j o u w a i i t e N k i d e s u n e",
    "output": "weather.wav"
}
```

## 🎓 Training Custom Models

### Prepare Dataset
```bash
python python/prepare_dataset.py \
    --input data/ja_phonemes.json \
    --output training_data/ \
    --split 0.8 0.1 0.1
```

### Train DeepPhonemizer
```bash
python python/train_phonemizer.py \
    --dictionary data/ja_phonemes.json \
    --output-dir training_output/ \
    --model-type forward \
    --epochs 50 \
    --batch-size 32 \
    --export-onnx
```

## 📁 Project Structure

```
jp-edge-tts/
├── include/jp_edge_tts/       # Public headers
│   ├── core/                  # Core engine headers
│   ├── phonemizer/            # G2P conversion headers
│   ├── tokenizer/             # Tokenization headers
│   ├── audio/                 # Audio processing headers
│   └── utils/                 # Utility headers
├── src/                       # Implementation files
├── examples/                  # Example applications
│   └── cli/                   # Command-line interface
├── python/                    # Python scripts
├── models/                    # Model files
├── data/                      # Data files
└── CMakeLists.txt             # CMake configuration
```

## 🤝 Contributing

We welcome contributions! Please see guidelines in the repository.

## 📄 License

This project is licensed under the MIT License.

---

<p align="center">Made with ❤️ for the Japanese language learning community</p>
