FROM resin/rpi-raspbian
ADD setup.sh /
RUN chmod +x /setup.sh
RUN /setup.sh
ADD zoo.cfg /usr/local/zookeeper/conf/zoo.cfg
ADD kafka-server-start.sh /usr/local/kafka/bin/kafka-server-start.sh
ADD modifykafka.sh /
# RUN /modifykafka.sh
RUN sudo /usr/local/zookeeper/bin/zkServer.sh start