#docker run --rm -v "${PWD}":/config --network host -it esphome/esphome run victron.yaml
# To flash via usb serial.
#docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB0 --network host -it esphome/esphome run victron.yaml
#docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB0 --network host -it esphome/esphome run daly.yaml
#docker run --rm -v "${PWD}":/config --device=/dev/ttyUSB0 --network host -it esphome/esphome run "$1"
docker run --rm -v "${PWD}":/config --network host -it esphome/esphome run "$1"
