# holooj-raspberry

## Raspberry

In pi, note the default swap size:
```bash
cat /etc/dphys-swapfile | grep CONF_SWAPSIZE
```
then change it to 1024:
```bash
sudo perl -pi -e 's/CONF_SWAPSIZE=\d+/CONF_SWAPSIZE=1024/'  /etc/dphys-swapfile
```

Install:

```bash
#libraries required from holooj
sudo apt install git libusb-1.0-0-dev libturbojpeg0-dev

#ncsdk only api
sudo nano /etc/dphys-swapfile
git clone -b ncsdk2 http://github.com/Movidius/ncsdk && cd ncsdk && make api

#rasp-webgui for pi as access point
wget -q https://git.io/voEUQ -O /tmp/raspap && bash /tmp/raspap

#holooj-raspbbery
git clone https://github.com/princio/holooj-raspberry.git && make all
```

On a Linux machine, not Raspberry, install Darkflow and execute:
```bash
flow --model yolov2.cfg --load yolov2.weights  --savepb
```

then convert to movidius format with mvNCCompile:
```bash
mvNCCompile yolov2.pb -s 12 -in input -on output -o yolov2.graph
```
Restore swap size:
```bash
sudo perl -pi -e 's/CONF_SWAPSIZE=\d+/CONF_SWAPSIZE=100/'  dphys-swapfile
```

### Execute:

In holooj-raspberry root folder:
```bash
./debug/gengi --iface wlan0 --graph ./yolo/yolov2-tiny.graph
```
where:
```
        --graph                 the path to the graph.
        --iface                 the network interface to use.
        --help, -h              this help.
```