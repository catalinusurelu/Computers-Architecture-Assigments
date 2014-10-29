"""
    Copyright: Usurelu Catalin Constantin, 333CA
"""

class Message:
    """
        Class reprezenting a message.
    """

    def __init__(self, **kwargs):
        """
            Constructor.

            @type type: String
            @param type: a string repezenting the type of this message
            @type column: Integer
            @param column: column for which this message is used to
                           update/request
            @type value: Float
            @param value: value corespoding to the element that this
                          message intends to update/requeset
            @type src: Node
            @param src: source Node of this message
        """
        
        self.type = kwargs.get('type')
        self.column = kwargs.get('column')
        self.value = kwargs.get('value')
        self.src = kwargs.get('src')

    def set(self, header, value):
        setattr(self, header, value)

# Aliases
Request = Message
Reply = Message
