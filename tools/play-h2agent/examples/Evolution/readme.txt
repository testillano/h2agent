Everytime we request a GET to the server with uri '/next',
we will obtain a evolving state within 3 different reactions.

Traffic classification (matching configuration) will be simple, FullMatching,
 as the example is done for an static/constant and hence, predictable URI.

Traffic behavior (provision configuration) will configure three provisions
with different response body correspoding to the processed state which evolves
from 'initial' through 'second' and finally 'third' one.
