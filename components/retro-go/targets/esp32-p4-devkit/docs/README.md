## Config
python rg_tool.py --target=esp32-p4-devkit --no-networking build launcher
cd launcher
idf.py menuconfig

## Build
python rg_tool.py --target=esp32-p4-devkit --no-networking build-img

## Clean
python rg_tool.py --target=esp32-p4-devkit --no-networking clean

## Flash
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking install

python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking run launcher
python rg_tool.py --target=esp32-p4-devkit --port=COM6 --no-networking flash retro-core
