FROM resin/rpi-raspbian
ADD zoo.cfg /
ADD kafka-server-start.sh /
ADD init.d.kafka /
ADD setup.sh /
RUN chmod +x /setup.sh
RUN /setup.sh
