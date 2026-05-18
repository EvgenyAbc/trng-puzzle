#!/bin/sh
set -e

echo "==> Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y libftdi1-dev libusb-dev

echo "==> Cloning infnoise repository..."
if [ -d /tmp/infnoise ]; then
    rm -rf /tmp/infnoise
fi
git clone --depth=1 https://github.com/leetronics/infnoise.git /tmp/infnoise

echo "==> Building libinfnoise..."
cd /tmp/infnoise/software
make -f Makefile.linux libinfnoise.so

echo "==> Installing libinfnoise..."
sudo make -f Makefile.linux install-lib
sudo ldconfig

echo "==> Installing infnoise tools..."
sudo make -f Makefile.linux install

echo "==> Installing udev rule..."
sudo cp init_scripts/75-infnoise.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "==> Blacklisting ftdi_sio kernel driver..."
echo "blacklist ftdi_sio" | sudo tee /etc/modprobe.d/blacklist-infnoise.conf
sudo modprobe -r ftdi_sio 2>/dev/null || true

echo "==> Building example..."
cd /home/quit/code/trng
make

echo "==> Testing..."
sudo systemctl stop infnoise 2>/dev/null || true
sleep 1
output=$(./trng-read 64 2>&1)
if [ $? -eq 0 ] && [ -n "$output" ]; then
    echo "SUCCESS: Read $(echo "$output" | wc -c) bytes from TRNG:"
    echo "$output" | xxd
else
    echo "FAILED: $output"
    exit 1
fi

echo "==> Restarting infnoise service..."
sudo systemctl start infnoise 2>/dev/null || true

echo ""
echo "Done! TRNG is working."
