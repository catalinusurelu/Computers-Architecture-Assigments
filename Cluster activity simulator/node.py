"""
    This module represents a cluster's computational node.

    Computer Systems Architecture Course
    Assignment 1 - Cluster Activity Simulation
    March 2014
"""

"""
    Student: Usurelu Catalin Constantin, 333CA
"""

from threading import Event, Thread
from Queue import Queue
from barrier import ReusableBarrierSem
from JobManager import JobManager
from EquationSystemSolver import EquationSystemSolver
from Messaging import *

class Node:
    """
        Class that represents a cluster node with computation and storage
        functionalities.
    """

    def __init__(self, node_id, matrix_size):
        """
            Constructor.

            @type node_id: Integer
            @param node_id: an integer less than 'matrix_size' uniquely
                identifying the node
            @type matrix_size: Integer
            @param matrix_size: the size of the matrix A
        """

        self.node_id = node_id
        self.matrix_size = matrix_size
        self.datastore = None
        self.nodes = None
        self.jobManager = None
        self.mainProcess = None
        self.received_queue = Queue()
        self.synchronous_request_buffer = Queue(1)  

    def __str__(self):
        """
            Pretty prints this node.

            @rtype: String
            @return: a string containing this node's id
        """
        return "Node %d" % self.node_id


    def set_datastore(self, datastore):
        """
            Gives the node a reference to its datastore. Guaranteed to be called
            before the first call to 'get_x'.

            @type datastore: Datastore
            @param datastore: the datastore associated with this node
        """
        self.datastore = datastore
        self.jobManager = JobManager(self, datastore)
        self.jobManager.start()

    def set_nodes(self, nodes):
        """
            Informs the current node of the other nodes in the cluster. 
            Guaranteed to be called before the first call to 'get_x'.

            @type nodes: List of Node
            @param nodes: a list containing all the nodes in the cluster
        """
        self.nodes = nodes

    def get_x(self):
        """
            Computes the x value corresponding to this node. This method is
            invoked by the tester. This method must block until the result is
            available.

            @rtype: (Float, Integer)
            @return: the x value and the index of this variable in the solution
                vector

            @type mainProcess: EquationSystemSolver
            @param nodes: instance of the thread executing the main algorithm
        """

        self.mainProcess = EquationSystemSolver(self, self.received_queue)
        self.mainProcess.start()
        self.mainProcess.join()

        return (self.mainProcess.x, self.node_id)

    def shutdown(self):
        """
            Instructs the node to shutdown (terminate all threads). This method
            is invoked by the tester. This method must block until all the
            threads started by this node terminate.
        """

        # Inchid thread-uri conexiuni
        self.jobManager.close()

    def get_A(self, column):
        """
            Synchronously (blocking operation) returns an element from the row
            of the A matrix that is stored in this datastore.

            @type column: Integer
            @param column: the column of the element

            @rtype: Float
            @return: the element of matrix A at the requested position
        """

        request = Request(type = "Get_A", column = column, src = self)
        self.process_request(request)
        return self.synchronous_request_buffer.get()


    def put_A(self, column, A):
        """
            Synchronously (blocking operation) updates an element from the row
            of the A matrix that is stored in this datastore.

            @type column: Integer
            @param column: the column of the element
            @type A: Float
            @param A: the new element value
        """

        request = Request(type = "Put_A", value = A, column = column, src = self)
        self.process_request(request)
        self.synchronous_request_buffer.get()   # Wait for operation to complete


    def get_b(self):
        """
            Synchronously (blocking operation) returns the element of b stored 
            in this datastore.

            @rtype: Float
            @return: the element of b stored in this datastore
        """

        request = Request(type = "Get_b", src = self)
        self.process_request(request)
        return self.synchronous_request_buffer.get()  

    def put_b(self, b):
        """
            Synchronously (blocking operation) updates the element of b stored
            in this datastore.

            @type b: Float
            @param b: the new value of b
        """

        request = Request(type = "Put_b", value = b, src = self)
        self.process_request(request)
        self.synchronous_request_buffer.get()  # Wait for operation to complete
   

    def process_request(self, request):
        """
            Route the request to the appropriate handler (jobManager or directly
            pass to the received_queue for direct use in the mainProcess
            instance)
        """

        if(request.type == "Barrier"):
            self.received_queue.put(request)
        else:
            self.jobManager.process_request(request)
