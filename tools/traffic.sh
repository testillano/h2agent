#!/bin/bash
cat << EOF

curl -v -XGET --http2-prior-knowledge http://localhost:8000/<uri path here>

Where, for example, uri path could be:

   app/v1/foo/bar/1
   "app/v1/foo/bar-1?name=john&city=Madrid"
   app/v1/id-555112244/ts-1615562841
   ...

EOF

