Shopping cart with scoped variable accumulation across outState chain.

Demonstrates var.* propagation across outState links in server mode.
Each POST to /cart adds an item price to a running total stored in var.total.
The running total is returned in every response.

All provisions share the same method+URI (POST /cart) because in FullMatching
mode, the state and chain variables are scoped to the DataKey (method + uri).

Flow:
  1. POST /cart {"price":10} -> inState=initial, sets var.total=10, outState=has-items
     Response: {"step":"first","running":10}
  2. POST /cart {"price":25} -> inState=has-items, accumulates var.total=35, outState=has-items
     Response: {"step":"added","running":35}
  3. POST /cart {"price":7}  -> inState=has-items, accumulates var.total=42, outState=has-items
     Response: {"step":"added","running":42}

Without scoped variables, this would require globalVar with seq-based naming
and manual eraser cleanup. With scoped var, no cleanup is needed — variables
are automatically destroyed when the chain ends (e.g., purge).
