#!/bin/bash

# ==========================================
# JP Edge TTS Build Script for Unix Systems
# ==========================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
INSTALL_PREFIX="/usr/local"
ENABLE_GPU=OFF
ENABLE_TESTS=ON
ENABLE_EXAMPLES=ON
PARALLEL_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check dependencies
check_dependencies() {
    print_info "Checking dependencies..."

    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake is not installed. Please install CMake 3.18 or higher."
        exit 1
    fi

    # Check CMake version
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)

    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 18 ]); then
        print_error "CMake version 3.18 or higher is required. Found: $CMAKE_VERSION"
        exit 1
    fi

    # Check for compiler
    if command -v g++ &> /dev/null; then
        COMPILER="g++"
        COMPILER_VERSION=$(g++ --version | head -n1)
        print_info "Found compiler: $COMPILER_VERSION"
    elif command -v clang++ &> /dev/null; then
        COMPILER="clang++"
        COMPILER_VERSION=$(clang++ --version | head -n1)
        print_info "Found compiler: $COMPILER_VERSION"
    else
        print_error "No C++ compiler found. Please install g++ or clang++."
        exit 1
    fi

    # Check for Git (for submodules)
    if ! command -v git &> /dev/null; then
        print_warning "Git is not installed. Cannot update submodules."
    fi

    # Check for Python (for training scripts)
    if command -v python3 &> /dev/null; then
        PYTHON_VERSION=$(python3 --version)
        print_info "Found Python: $PYTHON_VERSION"
    else
        print_warning "Python 3 not found. Training scripts will not be available."
    fi
}

# Function to update submodules
update_submodules() {
    if command -v git &> /dev/null && [ -d .git ]; then
        print_info "Updating git submodules..."
        git submodule update --init --recursive
    fi
}

# Function to install dependencies
install_dependencies() {
    print_info "Installing system dependencies..."

    if [ "$(uname)" == "Darwin" ]; then
        # macOS
        if command -v brew &> /dev/null; then
            print_info "Installing dependencies via Homebrew..."
            brew install cmake mecab nlohmann-json
        else
            print_warning "Homebrew not found. Please install dependencies manually."
        fi
    elif [ -f /etc/debian_version ]; then
        # Debian/Ubuntu
        print_info "Installing dependencies via apt..."
        sudo apt-get update
        sudo apt-get install -y cmake build-essential libmecab-dev mecab-ipadic-utf8 nlohmann-json3-dev
    elif [ -f /etc/redhat-release ]; then
        # RHEL/CentOS/Fedora
        print_info "Installing dependencies via yum/dnf..."
        if command -v dnf &> /dev/null; then
            sudo dnf install -y cmake gcc-c++ mecab mecab-devel json-devel
        else
            sudo yum install -y cmake gcc-c++ mecab mecab-devel json-devel
        fi
    elif [ -f /etc/arch-release ]; then
        # Arch Linux
        print_info "Installing dependencies via pacman..."
        sudo pacman -S --needed cmake base-devel mecab nlohmann-json
    else
        print_warning "Unknown Linux distribution. Please install dependencies manually."
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --relwithdebinfo)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --install-prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --enable-gpu)
            ENABLE_GPU=ON
            shift
            ;;
        --disable-tests)
            ENABLE_TESTS=OFF
            shift
            ;;
        --disable-examples)
            ENABLE_EXAMPLES=OFF
            shift
            ;;
        --jobs|-j)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --install-deps)
            install_dependencies
            exit 0
            ;;
        --clean)
            print_info "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
            exit 0
            ;;
        --help|-h)
            echo "JP Edge TTS Build Script"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug              Build in Debug mode"
            echo "  --release            Build in Release mode (default)"
            echo "  --relwithdebinfo     Build in RelWithDebInfo mode"
            echo "  --build-dir DIR      Set build directory (default: build)"
            echo "  --install-prefix DIR Set installation prefix (default: /usr/local)"
            echo "  --enable-gpu         Enable GPU support"
            echo "  --disable-tests      Disable building tests"
            echo "  --disable-examples   Disable building examples"
            echo "  --jobs N, -j N       Set number of parallel build jobs"
            echo "  --install-deps       Install system dependencies"
            echo "  --clean              Clean build directory"
            echo "  --help, -h           Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                   # Build with defaults"
            echo "  $0 --debug           # Build in debug mode"
            echo "  $0 --enable-gpu -j 8 # Build with GPU support using 8 cores"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Main build process
print_info "========================================="
print_info "JP Edge TTS Build Configuration"
print_info "========================================="
print_info "Build Type:        $BUILD_TYPE"
print_info "Build Directory:   $BUILD_DIR"
print_info "Install Prefix:    $INSTALL_PREFIX"
print_info "GPU Support:       $ENABLE_GPU"
print_info "Build Tests:       $ENABLE_TESTS"
print_info "Build Examples:    $ENABLE_EXAMPLES"
print_info "Parallel Jobs:     $PARALLEL_JOBS"
print_info "========================================="

# Check dependencies
check_dependencies

# Update submodules
update_submodules

# Create build directory
print_info "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_info "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTS="$ENABLE_TESTS" \
    -DBUILD_EXAMPLES="$ENABLE_EXAMPLES" \
    -DUSE_GPU="$ENABLE_GPU" \
    -DUSE_MECAB=ON \
    -DENABLE_PROFILING=OFF

# Build
print_info "Building with $PARALLEL_JOBS parallel jobs..."
cmake --build . --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

# Run tests if enabled
if [ "$ENABLE_TESTS" == "ON" ] && [ "$BUILD_TYPE" != "Debug" ]; then
    print_info "Running tests..."
    ctest --config "$BUILD_TYPE" --output-on-failure || print_warning "Some tests failed"
fi

print_info "========================================="
print_info "Build completed successfully!"
print_info "========================================="
print_info ""
print_info "To install the library, run:"
print_info "  cd $BUILD_DIR && sudo make install"
print_info ""
print_info "To run the CLI tool:"
print_info "  $BUILD_DIR/bin/jp_tts_cli \"こんにちは\""
print_info ""
print_info "To use the library in your project, add to CMakeLists.txt:"
print_info "  find_package(jp_edge_tts REQUIRED)"
print_info "  target_link_libraries(your_target jp_edge_tts::jp_edge_tts_core)"
print_info ""