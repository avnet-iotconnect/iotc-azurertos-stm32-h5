#!/bin/bash

set -e

pushd "$(dirname $0)"/../../ >/dev/null

cube_zip_name='en.x-cube-azure-h5-v1-0-0.zip'
sec_zip_name='en.x-cube-sec-m-h5-1.0.0.zip'

#git clean -fdx -e "${cube_zip_name}" -e  "${sec_zip_name}"

git submodule update --init --recursive

if [ ! -f ${cube_zip_name} ]; then
  echo "ERROR: The X-Cube Azure project needs to be downloaded as ${cube_zip_name} into ${PWD} "
  exit 2
fi
if [ ! -f ${sec_zip_name} ]; then
  echo "ERROR: The X-CUBE-SEC-M-H5 project needs to be downloaded from from https://www.st.com/en/embedded-software/stm32trustee-sm.html as ${cube_zip_name} into ${PWD} "
  exit 2
fi


(chmod -R a+w STM32CubeExpansion_Cloud_AZURE_* || true )2>/dev/null # the package has a problem where user has no write permissions
rm -rf STM32CubeExpansion_Cloud_AZURE_*
rm -rf X-Cube-SEC-M-H5_*

echo "Extracting files from ${cube_zip_name}..."
unzip -q -o "${cube_zip_name}"
pushd STM32CubeExpansion_Cloud_AZURE_*/ >/dev/null

echo "Copying files from the package over the repository files..."
cp -nr Drivers Middlewares Projects Utilities .. # do not overwrite existing files

# remove the unsupported project
rm -rf Projects/B-U585I-IOT02A/Applications/NetXDuo


popd >/dev/null

echo "Extracting files from ${sec_zip_name}..."
unzip -q -o "${sec_zip_name}"
pushd X-Cube-SEC-M-H5_*/ >/dev/null

echo "Copying files from the package over the repository files..."
cp -nr Projects .. # do not overwrite existing files

popd >/dev/null


echo "Cleaning up..."
chmod -R a+w STM32CubeExpansion_Cloud_AZURE_* # the package has a problem where user has no write permissions
rm -rf STM32CubeExpansion_Cloud_AZURE_*
rm -rf X-Cube-SEC-M-H5_*

echo "Done."