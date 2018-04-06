from .forwarderconfig import ForwarderConfig
from confluent_kafka import Producer, KafkaError, Consumer, KafkaException
import uuid


class ProducerWrapper:
    """
    A wrapper class for the kafka producer.
    """

    def __init__(self, server, config_topic, data_topic):
        self.topic = config_topic
        self.converter = ForwarderConfig(data_topic)
        self._set_up_producer(server)

    def _set_up_producer(self, server):
        conf = {'bootstrap.servers': server}
        try:
            self.producer = Producer(**conf)
            conf['group.id'] = uuid.uuid4()
            self.consumer = Consumer(**conf)
            if not self.topic_exists(self.topic):
                print("WARNING: topic {} does not exist. It will be created by default.".format(self.topic))
        except KafkaException.args[0] == '_BROKER_NOT_AVAILABLE':
            print("No brokers found on server: " + server[0])
            quit()
        except KafkaException.args[0] == '_TIMED_OUT':
            print("No server found, connection error")
            quit()
        except KafkaException.args[0] == '_INVALID_ARG':
            print("Invalid configuration")
            quit()
        except KafkaException.args[0] == '_UNKNOWN_TOPIC':
            print("Invalid topic, to enable auto creation of topics set"
                  " auto.create.topics.enable to false in broker configuration")
            quit()

    def add_config(self, pvs):
        """
        Creates a forwarder configuration to add more pvs to be monitored.

        Args:
             pvs (list): A list of new PVs to add to the forwarder configuration.

        Returns:
            None.
        """

        data = self.converter.create_forwarder_configuration(pvs)
        print("Sending data {}".format(data))
        self.producer.produce(self.topic, value=data)

    def topic_exists(self, topicname):
        try:
            self.consumer.subscribe([topicname])
        except KafkaException as e:
            print("topic '{}' does not exist".format(topicname))
            print(e)
            return False
        return True

    def remove_config(self, pvs):
        """
        Creates a forwarder configuration to remove pvs that are being monitored.

        Args:
            pvs (list): A list of PVs to remove from the forwarder configuration.

        Returns:
            None.
        """

        data = self.converter.remove_forwarder_configuration(pvs)
        for pv in data:
            print("Sending data {}".format(data))
            self.producer.produce(self.topic, key=pv, value="")
