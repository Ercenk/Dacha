# warm up
sudo apt-get update
sudo apt-get install wget

# Install zookeper
wget http://mirrors.ocf.berkeley.edu/apache/zookeeper/zookeeper-3.4.10/zookeeper-3.4.10.tar.gz
sudo tar -zxvf  zookeeper-3.4.10.tar.gz  -C /usr/local
sudo mv /usr/local/zookeeper-3.4.10/ /usr/local/zookeeper
rm zookeeper-3.4.10.tar.gz

# Install kafka
pwd
cd ~
pwd
wget https://www.apache.org/dyn/closer.cgi?path=/kafka/0.11.0.1/kafka_2.11-0.11.0.1.tgz
ls
ls kafka_2.11-0.11.0.1.tgz
sudo tar -zxvf kafka_2.11-0.11.0.1.tgz -C /usr/local/
sudo mv /usr/local/kafka_2.11-0.11.0.1/ /usr/local/kafka
rm kafka_2.11-0.11.0.1.tgz
