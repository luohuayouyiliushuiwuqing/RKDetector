#!/bin/bash

set -e

echo "$0 $@"
while getopts ":d:b:m:r" opt; do
  case $opt in
    b)
      BUILD_TYPE=$OPTARG
      ;;
    m)
      ENABLE_ASAN=ON
      export ENABLE_ASAN=TRUE
      ;;
    d)
      BUILD_DEMO_NAME=$OPTARG
      ;;
    r)
      DISABLE_RGA=ON
      ;;
    :)
      echo "Option -$OPTARG requires an argument."
      exit 1
      ;;
    ?)
      echo "Invalid option: -$OPTARG index:$OPTIND"
      ;;
  esac
done

if [ -z ${BUILD_DEMO_NAME} ]; then
  echo "$0 -d <build_demo_name> [-b <build_type>] [-m] [-r]"
  echo ""
  echo "    -d : demo name"
  echo "    -b : build_type(Debug/Release)"
  echo "    -m : enable address sanitizer, build_type need set to Debug"
  echo "    -r : disable rga, use cpu resize image"
  echo "such as: $0 -d yolov8"
  echo ""
  exit -1
fi

TARGET_SOC=rk3588
TARGET_ARCH=aarch64
GCC_COMPILER=aarch64-linux-gnu

echo "$GCC_COMPILER"
export CC=${GCC_COMPILER}-gcc
export CXX=${GCC_COMPILER}-g++

if command -v ${CC} >/dev/null 2>&1; then
    :
else
    echo "${CC} is not available"
    echo "Please install aarch64-linux-gnu toolchain"
    exit
fi

# Debug / Release
if [[ -z ${BUILD_TYPE} ]];then
    BUILD_TYPE=Release
fi

# Build with Address Sanitizer for memory check, BUILD_TYPE need set to Debug
if [[ -z ${ENABLE_ASAN} ]];then
    ENABLE_ASAN=OFF
fi

if [[ -z ${DISABLE_RGA} ]];then
    DISABLE_RGA=OFF
fi

for demo_path in `find ./ -name ${BUILD_DEMO_NAME}`
do
    if [ -d "$demo_path/cpp" ]
    then
        BUILD_DEMO_PATH="$demo_path/cpp"
        break;
    fi
done

if [[ -z "${BUILD_DEMO_PATH}" ]]
then
    echo "Cannot find demo: ${BUILD_DEMO_NAME}, only support:"

    for demo_path in `find ./ -name cpp`
    do
        if [ -d "$demo_path" ]
        then
            dname=`dirname "$demo_path"`
            name=`basename $dname`
            echo "$name"
        fi
    done
    exit
fi

TARGET_SDK="rknn_${BUILD_DEMO_NAME}_demo"

TARGET_PLATFORM=${TARGET_SOC}_linux_${TARGET_ARCH}
ROOT_PWD=$( cd "$( dirname $0 )" && cd -P "$( dirname "$SOURCE" )" && pwd )
INSTALL_DIR=${ROOT_PWD}/install/${TARGET_PLATFORM}/${TARGET_SDK}
BUILD_DIR=${ROOT_PWD}/build/build_${TARGET_SDK}_${TARGET_PLATFORM}_${BUILD_TYPE}

echo "==================================="
echo "BUILD_DEMO_NAME=${BUILD_DEMO_NAME}"
echo "BUILD_DEMO_PATH=${BUILD_DEMO_PATH}"
echo "TARGET_SOC=${TARGET_SOC}"
echo "TARGET_ARCH=${TARGET_ARCH}"
echo "BUILD_TYPE=${BUILD_TYPE}"
echo "ENABLE_ASAN=${ENABLE_ASAN}"
echo "DISABLE_RGA=${DISABLE_RGA}"
echo "INSTALL_DIR=${INSTALL_DIR}"
echo "BUILD_DIR=${BUILD_DIR}"
echo "CC=${CC}"
echo "CXX=${CXX}"
echo "==================================="

if [[ ! -d "${BUILD_DIR}" ]]; then
  mkdir -p ${BUILD_DIR}
fi

if [[ -d "${INSTALL_DIR}" ]]; then
  rm -rf ${INSTALL_DIR}
fi

cd ${BUILD_DIR}
cmake ../../${BUILD_DEMO_PATH} \
    -DTARGET_SOC=${TARGET_SOC} \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DENABLE_ASAN=${ENABLE_ASAN} \
    -DDISABLE_RGA=${DISABLE_RGA} \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
make -j4
make install

# Check if there is a rknn model in the install directory
suffix=".rknn"
shopt -s nullglob
if [ -d "$INSTALL_DIR" ]; then
    files=("$INSTALL_DIR/model/"/*"$suffix")
    shopt -u nullglob

    if [ ${#files[@]} -le 0 ]; then
        echo -e "\e[91mThe RKNN model can not be found in \"$INSTALL_DIR/model\", please check!\e[0m"
    fi
else
    echo -e "\e[91mInstall directory \"$INSTALL_DIR\" does not exist, please check!\e[0m"
fi
