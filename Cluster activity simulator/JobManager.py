"""
    Copyright: Usurelu Catalin Constantin, 333CA
"""


from threading import Event, Thread, Semaphore
from Queue import Queue
from barrier import ReusableBarrierSem
from Messaging import *

class Connection(Thread):
    """
        Class that reprezents the Woker handling the jobs for this node.
        Each worker uses ONLY one connection to the database at any
        given time.
   """

    def __init__(self, id, jobManager):
        """
            Constructor.

            @type id: Integer
            @param id: id for this worker
            @type jobManager: JobManager
            @param jobManager: the jobManager that coordinates the woker
                               threads.
        """

        Thread.__init__(self)

        self.id = id
        self.node = jobManager.node
        self.jobManager = jobManager
        self.datastore = jobManager.datastore

        self.datastore.register_thread(self.node, self)

    def run(self):
        """
            Start worker thread. 
            Observation: each request is verified to see if it's either
            synchronous or asynchronous. All requests originating from
            the node owning this worker are synchronous.
        """

        # Not busy-waiting, we use a synchronised queue to wait for requests:)
        while True:
            request = self.jobManager.request_queue.get()

            """
                Process each type of request.
            """

            if request.type == "Get_A":
                x = self.datastore.get_A(self.node, request.column)

                # Synchronous
                if request.src.node_id == self.node.node_id:
                    self.node.synchronous_request_buffer.put(x)
                # Asynchronous
                else:
                    reply = Reply(
                        type = "Get_A",
                        column = request.column,
                        value = x, src = self.node)
                    request.src.mainProcess.received_queue.put(reply)

            elif request.type == "Put_A":
                self.datastore.put_A(self.node, request.column, request.value)
                if request.src.node_id == self.node.node_id:
                    reply = Reply(type = "Confirm_Put", src = self.node)
                    self.node.synchronous_request_buffer.put(reply)

            elif request.type == "Get_b":
                x = self.datastore.get_b(self.node)

                if request.src.node_id == self.node.node_id:
                    self.node.synchronous_request_buffer.put(x)
                else:
                    reply = Reply(type = "Get_b", value = x, src = self.node)
                    request.src.mainProcess.received_queue.put(reply)

            elif request.type == "Put_b":
                self.datastore.put_b(self.node, request.value)
                if request.src.node_id == self.node.node_id:
                    reply = Reply(type = "Confirm_Put", src = self.node)
                    self.node.synchronous_request_buffer.put(reply)

            elif request.type == "Swap":

                # Swap b's
                if request.column == self.node.matrix_size:
                    x = self.datastore.get_b(self.node)
                # Swap coefficients
                else:
                    x = self.datastore.get_A(self.node, request.column)
                reply = Reply(
                        type = "Swap_phase_1",
                        src = self.node,
                        column = request.column,
                        value = x)
                request.src.process_request(reply)

            elif request.type == "Swap_phase_1":
                if request.column == self.node.matrix_size:
                    x = self.datastore.get_b(self.node)
                    self.datastore.put_b(self.node, request.value)
                else:
                    x = self.datastore.get_A(self.node, request.column)
                    self.datastore.put_A(self.node, request.column, request.value)

                reply = Reply(
                        type = "Swap_phase_2",
                        src = self.node,
                        column = request.column,
                        value = x)
                request.src.process_request(reply)

            elif request.type == "Swap_phase_2":
                if request.column == self.node.matrix_size:
                    self.datastore.put_b(self.node, request.value)
                else:
                    self.datastore.put_A(self.node, request.column, request.value)

                # Inform both the "sender" and "receiver" of the swap operations
                # that it is done so that they can wait at a barrier until
                # the swap operation is done.
                reply = Reply(type = "Swap_done", src = self.node)
                request.src.mainProcess.received_queue.put(reply)
                self.node.mainProcess.received_queue.put(reply)

            # Terminate thread
            elif request.type == "Close":
                return


class JobManager:
    """
        Class that represents a Job Manager implementing the Replicated Workers
        paradigm.
    """

    def __init__(self, node, datastore):
        """
            Constructor.

            @type node: Node
            @param node: Node instance on which this jobManager works
            @type datastore: Datastore
            @param datastore: the datastore associated with this jobManager'self
                              node.
        """

        self.node = node
        self.datastore = datastore
        self.request_queue = Queue()
        self.connections = []

    def start(self):
        """
            Start worker threads. The number of worker threads is either the
            given number of maximum database connexions or is equal to the
            number of nodes in the cluster in case of infinite conexions - this
            is sufficient, we usualy don't get more request than this.
        """

        if self.datastore.get_max_pending_requests() == 0:
            nr_connections = self.node.matrix_size
        else:
            nr_connections = self.datastore.get_max_pending_requests()

        for i in xrange(nr_connections):
            self.connections.append(Connection(i, self))
            self.connections[-1].start()

    def process_request(self, request):
        """
            Puts request in the request_queue so that an available worker can
            get the request and process it.

            @type request: Request
            @param request: request to be processed by an available worker.
        """

        self.request_queue.put(request)

    def close(self):
        """
            Sends messages to workers informing them to cut connections and
            finish working.
        """

        # Send Close requests
        for connection in self.connections:
            self.request_queue.put(Request(type = "Close"))

        # Wait for all workers to finish
        for connection in self.connections:
            connection.join()


