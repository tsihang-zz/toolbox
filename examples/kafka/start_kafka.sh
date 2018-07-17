kafka-server-stop.sh
sleep 3
zookeeper-server-stop.sh
sleep 3

zookeeper-server-start.sh $KAFKA_HOME/config/zookeeper.properties &
sleep 5
kafka-server-start.sh $KAFKA_HOME/config/server.properties &
