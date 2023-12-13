#!/bin/bash

set -e

pushd "$(dirname $0)"/../../ >/dev/null

if [ ! -f Projects/STM32H573I-DK/Applications/ROT/Nx_Azure_IoT/Binary/appli_enc_sign.hex ]; then
  echo "ERROR: You must first build the project!"
fi

rm -f stm32h5-binary.zip download.* provisioning.* regression.*

cat > download.sh <<END
#/bin/bash
echo Running the script. This can take some time...
cd Projects/STM32H573I-DK/Applications/ROT/Nx_Azure_IoT/
./download.sh
killall download.sh || true
read -n 1 -s -r -p "Press any key to continue"
END

cat > provisioning.sh <<END
#/bin/bash
echo Running the script. This can take some time...
cd Projects/STM32H573I-DK/ROT_Provisioning/SM/
./provisioning.sh AUTO
read -n 1 -s -r -p "Press any key to continue"
END

cat > regression.sh <<END
echo Running the script. This can take some time...
#/bin/bash
cd Projects/STM32H573I-DK/ROT_Provisioning/DA/
./regression.sh AUTO
read -n 1 -s -r -p "Press any key to continue"
END


chmod a+x download.sh provisioning.sh regression.sh

{ find \
  Projects/ -type f \( \
     ! -name '*.c' -a ! -name '*.h'  -a ! -name '*.cpp' -a ! -name '*.hpp' -a ! -name '*.o' \
  -a ! -name '*.htm*' -a ! -name '*.png'  -a ! -name '*.svg'   -a ! -name '*.jpg' -a ! -name '*.css'     \
  -a ! -name '*.map*' -a ! -name '*.list' -a ! -name '*.du'    -a ! -name '*.d'   -a ! -name '*.s'       \
  -a ! -name '*.su'   -a ! -name '*.ld'   -a ! -name '*.cyclo' -a ! -name '*.mk'  -a ! -name '*akefile*' \
  -a ! -name '*.log' -a ! -name '*.gitignore' \
  \) ; \
  find \
  . -maxdepth 1 -type f -name '*.sh' ; \
  find \
  Utilities \( \
    ! -name '*.htm*' -a ! -name '*.png'  -a ! -name '*.svg'   -a ! -name '*.jpg' -a ! -name '*.css' \
    ! -name '*.zip' \
  \); \
} | xargs zip stm32h5-binary.zip

#rm -f download.* provisioning.* regression.*