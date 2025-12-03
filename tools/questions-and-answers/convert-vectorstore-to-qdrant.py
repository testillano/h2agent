import chromadb
from qdrant_client import QdrantClient, models
from qdrant_client.models import Distance, VectorParams
import os
import sys

# --- Configuration ---
CHROMA_PERSIST_DIR = "vectorstore-data"
CHROMA_COLLECTION_NAME = "h2agent"
QDRANT_COLLECTION_NAME = "h2agent-test"
QDRANT_URL = os.getenv("QDRANT_URL")
QDRANT_API_KEY = os.getenv("QDRANT_API_KEY")

if not QDRANT_URL:
   print("\nWARNING: missing 'QDRANT_URL' environment variable (try: source source.me.bash). Press ENTER to continue, CTRL-C to abort ...")
   input()

if not QDRANT_API_KEY:
   print("\nWARNING: missing 'QDRANT_API_KEY' environment variable. Press ENTER to continue, CTRL-C to abort ...")
   input()

# --- 1. Connection and chroma load ---
print(f"Cargando ChromaDB desde: {CHROMA_PERSIST_DIR}")
try:
    chroma_client = chromadb.PersistentClient(path=CHROMA_PERSIST_DIR)
    chroma_collection = chroma_client.get_collection(name=CHROMA_COLLECTION_NAME)

except Exception as e:
    print(f"ERROR loading chroma collection (confirm collection name !): {e}")
    exit()

# --- 2. Chroma data extraction ---
print("Extracting data from Chroma ...")

# 2.1 Retrieve all collection data (including embeddings, metadata and documents)
chroma_data = chroma_collection.get(
    ids=chroma_collection.get()['ids'], # obtain all the IDs
    include=['embeddings', 'metadatas', 'documents']
)

# Vector parameters (need for embedding size and distance metric)
# Chroma does not store dimension: we will deduce from first vector
VECTOR_SIZE = len(chroma_data['embeddings'][0])
DISTANCE_METRIC = Distance.COSINE # assume the most common one (anyway you should verify which one Chrome used ...)

print(f"Vector detected dimension: {VECTOR_SIZE}. Assumed metric: {DISTANCE_METRIC.value}")
print(f"Total of doccuments to migrate: {len(chroma_data['ids'])}")

# --- 3. Qdrant connection/preparation ---
qdrant_client = QdrantClient(url=QDRANT_URL, api_key=QDRANT_API_KEY)

print(f"Creating collection '{QDRANT_COLLECTION_NAME}' in Qdrant ...")
# create from scratch:
if qdrant_client.collection_exists(collection_name=QDRANT_COLLECTION_NAME):
    qdrant_client.delete_collection(collection_name=QDRANT_COLLECTION_NAME)

qdrant_client.create_collection(
    collection_name=QDRANT_COLLECTION_NAME,
    vectors_config=VectorParams(size=VECTOR_SIZE, distance=DISTANCE_METRIC)
)

# --- 4. Load data in Qdrant (batches) ---
print("Initiating data load in Qdrant ...")
points = []
batch_size = 1000
for i, doc_id in enumerate(chroma_data['ids']):
    # Create payload combining metadata and original document
    payload = chroma_data['metadatas'][i] or {}
    payload['text'] = chroma_data['documents'][i] # original text is key for RAG

    # Qdrant point
    point = models.PointStruct(
        id=doc_id, # Reuse Chroma ID
        vector=chroma_data['embeddings'][i],
        payload=payload
    )
    points.append(point)

    # Upload batches
    if len(points) >= batch_size:
        qdrant_client.upsert(
            collection_name=QDRANT_COLLECTION_NAME,
            wait=True,
            points=points
        )
        print(f"Uploaded {i+1} points...")
        points = []

# Remaining batch
if points:
    qdrant_client.upsert(
        collection_name=QDRANT_COLLECTION_NAME,
        wait=True,
        points=points
    )
    print(f"Uploaded {len(chroma_data['ids'])} total points.")

print("✅ Migration completed from Chroma to Qdrant.")

