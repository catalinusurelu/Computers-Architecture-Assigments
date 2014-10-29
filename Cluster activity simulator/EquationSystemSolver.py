"""
    Copyright: Usurelu Catalin Constantin, 333CA
"""


from threading import Event, Thread
from Queue import Queue
from barrier import ReusableBarrierSem
from Messaging import *

class EquationSystemSolver(Thread):
    """
        Class that reprezents the main process executing the equation system
        solving algorithm for this node.
   """

    def __init__(self, node, received_queue):
        """
            Constructor.

            @type node: Node
            @param node: Node on which the algorithm runs
            @type received_queue: Queue
            @param received_queue: the node's synchronized queue - waits for
                                   requests/replies from other nodes.
        """

        Thread.__init__(self)

        self.node = node
        self.nodes = self.node.nodes
        self.received_queue = received_queue
        self.barrier = None
        self.setSwap = False
        self.x = None

        self.node.datastore.register_thread(self.node, self)

        # Node 0 is a Leader node, so he is the one who creates the distributed
        # barrier and shares it with the other nodes
        if self.node.node_id == 0:
            self.barrier = ReusableBarrierSem(len(self.node.nodes))

    def send_message(self, request, node):
        """
            Sends the message to the destination node, and adds a source node
            header.

            @type request: Request
            @param request: request to be sent.
            @type node: Node
            @param node: destination node.
        """

        request.set("src", self.node)
        node.process_request(request)

    def recv_message(self):
        """
            Receives a message.
        """
        return self.received_queue.get()

    def run(self):
        """
            Start thread executing the main algorithm
        """

        # Leader sends barrier to other nodes
        if self.node.node_id == 0:
            for node in self.nodes:
                if node.node_id != self.node.node_id:
                    request = Request(type = "Barrier", value = self.barrier)
                    self.send_message(request, node)
        # Non-leader nodes receive barrier from leader
        else:
            request = self.recv_message()
            if request.type == "Barrier":
                self.barrier = request.value

        # Wait for all nodes to receive barrier
        self.barrier.wait()

        # At each step of the algorithm, the node coresponding to the current
        # step executes swap operations (find pivot) while the other nodes
        # apply an operation (multiply pivot by factor and substract it from
        # the lines the node reprezents, so that we zero out everything
        # underneath the column coresponding to this step)
        for k in xrange(self.node.matrix_size):

            if self.node.node_id == k:

                # Find pivot for current column
                val_max = self.node.get_A(k)
                i_max = k

                # Send request for the first column
                for i in xrange(k + 1, self.node.matrix_size):
                    request = Request(type = "Get_A", column = k)
                    self.send_message(request, self.nodes[i])

                # Receive first column (every node sents the element it owns)
                for i in xrange(k + 1, self.node.matrix_size):
                    reply = self.recv_message()
                    if val_max < reply.value:
                        val_max = reply.value
                        i_max = reply.src.node_id

                # Wait for the pivot finding operation to complete (otherwise
                # threads not coresponding to this step might go ahead and
                # do bad stuff)
                self.barrier.wait()

                # Inform the coresponding node that we want to swap lines
                if k != i_max:
                    self.nodes[i_max].mainProcess.setSwap = True
                
                # Wait for informing the node that we want to swap
                self.barrier.wait()

                # We only swap with other nodes
                # (it's useless to swap with ourselves)
                if k != i_max:

                    # Send swap request to the jobManager of the node - he takes
                    # care of the rest
                    for column in xrange(k, self.node.matrix_size + 1):
                        request = Request(type = "Swap", column = column)
                        self.send_message(request, self.nodes[i_max])

                    # Wait for swap finish confirmation (for every column)
                    for column in xrange(k, self.node.matrix_size + 1):
                        self.recv_message()

                # Wait for swap operation to finish
                self.barrier.wait()

                # Wait for Gaussian elimination algorithm to finish
                self.barrier.wait()

            else:

                 # Wait for the pivot finding operation to complete
                self.barrier.wait()

                # Wait for information of the appropriate node
                # about the swap operation
                self.barrier.wait()

                # If we are the swap destination, clear setSwap just in case we
                # will swap again in the future.
                if self.setSwap == True:
                    self.setSwap = False

                    # Wait for swap finish confirmation (for every column)
                    for column in xrange(k, self.node.matrix_size + 1):
                            self.recv_message()

                # Wait for swap operation to finish
                self.barrier.wait()

                if self.node.node_id > k:

                    # Compute factor and replace column coresponding to this
                    # step with zeroes (beneath this row)

                    request = Request(type = "Get_A", column = k)
                    self.send_message(request, self.nodes[k])
                    reply = self.recv_message()

                    # We use variables such as Akk for readability (names
                    # identical to the ones on the wiki page for increased
                    # readability)
                    Akk = reply.value
                    Aik = self.node.get_A(k)
                    factor = Aik / Akk
                    self.node.put_A(k, Aik - Akk * factor)

                    if(factor != 0):
                        for j in xrange(k + 1, self.node.matrix_size):
                            Aij = self.node.get_A(j)
                            request = Request(type = "Get_A", column = j)
                            self.send_message(request, self.nodes[k])
                            reply = self.recv_message()
                            Akj = reply.value

                            Aij = Aij - Akj * factor
                            self.node.put_A(j, Aij)

                        self.send_message(Request(type = "Get_b"), self.nodes[k])
                        reply = self.recv_message()
                        bk = reply.value
                        bi = self.node.get_b()
                        bi = bi - bk * factor
                        self.node.put_b(bi)

                # Wait for Gaussian elimination algorithm to finish
                self.barrier.wait()

        # Compute final X's

        self.x = self.node.get_b()

        # Receive values for other X's so we can compue our X
        for j in xrange(self.node.node_id + 1, self.node.matrix_size):
            reply = self.recv_message()
            xj = reply.value
            Aij = self.node.get_A(reply.src.node_id)

            self.x -= xj*Aij

        Aii = self.node.get_A(self.node.node_id)
        self.x /= Aii

        # Inform other nodes of our computed X value
        for j in reversed(xrange(self.node.node_id)):
            self.nodes[j].mainProcess.received_queue.put(Reply(
                                                        type = "X",
                                                        value = self.x,
                                                        src = self.node))

        # Wait for rest of the algorithm to finish
        self.barrier.wait()

