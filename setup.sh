# warm up
sudo apt-get update
sudo apt-get install wget oracle-java8-jdk

# Install zookeper
cd ~
wget http://mirrors.ocf.berkeley.edu/apache/zookeeper/zookeeper-3.4.10/zookeeper-3.4.10.tar.gz
sudo tar -zxvf  zookeeper-3.4.10.tar.gz  -C /usr/local
sudo mv /usr/local/zookeeper-3.4.10/ /usr/local/zookeeper
rm zookeeper-3.4.10.tar.gz

# Install kafka
cd ~
wget http://mirrors.gigenet.com/apache/kafka/0.11.0.1/kafka_2.11-0.11.0.1.tgz
sudo tar -zxvf kafka_2.11-0.11.0.1.tgz -C /usr/local/
sudo mv /usr/local/kafka_2.11-0.11.0.1/ /usr/local/kafka
rm kafka_2.11-0.11.0.1.tgz

# Copy files
sudo mv /zoo.cfg /usr/local/zookeeper/conf/zoo.cfg
sudo sed -i '/$# -lt 1/ i export JMX_PORT=${JMX_PORT:-9999} \nexport KAFKA_HEAP_OPTS="-Xmx256M -Xms128M"\n' /usr/local/kafka/bin/kafka-server-start.sh

sudo sed -i -e 's/KAFKA_JVM_PERFORMANCE_OPTS="-server -XX:+UseG1GC -XX:MaxGCPauseMillis=20 -XX:InitiatingHeapOccupancyPercent=35 -XX:+DisableExplicitGC -Djava.awt.headless=true"/KAFKA_JVM_PERFORMANCE_OPTS="-client -XX:+UseParNewGC -XX:+UseConcMarkSweepGC -XX:+CMSClassUnloadingEnabled -XX:+CMSScavengeBeforeRemark -XX:+DisableExplicitGC -Djava.awt.headless=true"/g' /usr/local/kafka/bin/kafka-run-class.sh 

sudo mv /init.d.kafka /etc/init.d/kafka
sudo chmod 755 /etc/init.d/kafka
sudo update-rc.d kafka defaults