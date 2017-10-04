#! /bin/sh

wget github.com/Motion-Project/motion/releases/download/release-4.0.1/pi_jessie_motion_4.0.1-1_armhf.deb 

sudo apt-get install gdebi-core
sudo gdebi pi_jessie_motion_4.0.1-1_armhf.deb
mkdir ~/.motion
cp /etc/motion/motion.conf ~/.motion/motion.conf
 nano ~/.motion/motion.conf
 