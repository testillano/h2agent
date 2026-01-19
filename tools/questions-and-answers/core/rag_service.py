import os, sys, glob, pickle
from langchain_community.document_loaders import (
    UnstructuredMarkdownLoader,
)  # pip3 install --upgrade requests
#from langchain.text_splitter import MarkdownTextSplitter
from langchain_text_splitters import MarkdownTextSplitter

# from langchain_community.document_loaders import PyPDFLoader
# from langchain.text_splitter import CharacterTextSplitter
from langchain_openai import OpenAIEmbeddings
from langchain_openai import OpenAI
from langchain_groq import ChatGroq

from langchain.chains import (
    ConversationalRetrievalChain,
)  # RetrievalQA is more simple (no context)

from langchain_chroma import Chroma
from langchain_qdrant import QdrantVectorStore # used when QDRANT_URL is defined
from qdrant_client import QdrantClient

# Uncomment for debug purposes:
#from langchain.globals import set_verbose
#set_verbose(True)

import warnings
# Qdrant insecure API Key warning
warnings.filterwarnings(
    "ignore",
    message="Api key is used with an insecure connection.",
    category=UserWarning
)

OPENAI_EMBEDDINGS_MODEL="text-embedding-3-small" # dimension 1536
#OPENAI_EMBEDDINGS_MODEL="text-embedding-3-large" # dimension 3072
#OPENAI_EMBEDDINGS_MODEL="text-embedding-ada-002"

# Script location
SCR_PATH = os.path.abspath(__file__)
SCR_DIR, SCR_BN = os.path.split(SCR_PATH)
TARGET_DIR = os.path.abspath(SCR_DIR + "/../../..")

# API KEYs:
def _has_apikey(key, mandatory=True):
    result = False
    try:
        dummy = os.environ[key]
        result = True
    except:
        if mandatory:
            print(f"ERROR: you must define '{key}' environment variable")
            sys.exit(1)
        else:
            print(f"WARNING: you may want to define '{key}' environment variable")
    return result

class RAGService:
    """RAG logic, VectorDB and conversational retrieval chain."""

    DEFAULT_COLLECTION_NAME = 'h2agent'

    def __init__(
        self, vectorstore_path: str = "./vectorstore-data", target_dir: str = TARGET_DIR
    ):

        print("--- Inicializando RAG Service ---")

        # Chequeos de API KEY
        _has_apikey("OPENAI_API_KEY")
        self.has_groq = _has_apikey("GROQ_API_KEY", False)

        # Embeddings
        self.embeddings = OpenAIEmbeddings(
            model=OPENAI_EMBEDDINGS_MODEL,
            openai_api_base="http://localhost:5050/v1",
            #dimensions=1536 # must coincide with Qdrant !
        )


        # Detect database type: chroma/qdrant
        qdrant_url = os.environ.get("QDRANT_URL")
        if qdrant_url:
            self._init_qdrant(qdrant_url)
        else:
            self._init_chroma_local(vectorstore_path, target_dir)

        # Retriever
        retriever = self.vectorstore.as_retriever(
            search_type="similarity", search_kwargs={"k": 9}
        )

        # Language Model (LLM)
        if self.has_groq:
            self.llm = ChatGroq(
                model="llama-3.3-70b-versatile",
                temperature=0,
                max_tokens=None,
                timeout=None,
                max_retries=2,
            )
        else:
            self.llm = OpenAI(temperature=0,
                openai_api_base="http://localhost:5050/v1",
            )

        # Conversational Retrieval Chain
        self.qa_chain = ConversationalRetrievalChain.from_llm(self.llm, retriever)

        print("--- RAG Service initialized ---")

    def _init_chroma_local(self, vectorstore_path: str, target_dir: str):
        """Initializes the local chroma database."""

        # Load/create vectostore
        if not os.path.exists(vectorstore_path):
            print("Creating vectorstore ...")

            wildcard = target_dir + "/**/*.md"
            all_files = glob.glob(wildcard, recursive=True)

            exclude_patterns = [
                "pull_request_template.md",
                "AggregatedKeys.md",
                "tools/questions-and-answers/README.md",
            ]
            files = [
                f
                for f in all_files
                if not any(pattern in f for pattern in exclude_patterns)
            ]
            print(f"Files to process ({len(files)}):")

            print("\nWait while embeddings are created ...")
            loaders = [UnstructuredMarkdownLoader(f) for f in files]

            documents = []
            for loader in loaders:
                documents.extend(loader.load())

            # Split the document into chunks:
            text_splitter = MarkdownTextSplitter(chunk_size=2500, chunk_overlap=250)
            fragments = text_splitter.split_documents(documents)
            # For PDF files:
            # text_splitter = CharacterTextSplitter(separator="\n", chunk_size=1000, chunk_overlap=150, length_function=len)
            # fragments = text_splitter.split_documents(documents)

            # Create the vectorstore
            self.vectorstore = Chroma.from_documents(
                fragments,
                self.embeddings,
                persist_directory=vectorstore_path,
                collection_name=self.DEFAULT_COLLECTION_NAME,
            )

        else:
            print("Loading vectorstore ...")
            self.vectorstore = Chroma(
                persist_directory=vectorstore_path,
                embedding_function=self.embeddings,
                collection_name=self.DEFAULT_COLLECTION_NAME,  # Necesario si se usó en la creación
            )

        print(f"Vectorstore documents: {self.vectorstore._collection.count()}")
        print(f"Collection name: {self.vectorstore._collection.name}")

    def _init_qdrant(self, url: str):
        """Initializes the remote qdrant database."""

        print(f"Connecting remote Qdrant database on {url} ...")

        collection_name = self.DEFAULT_COLLECTION_NAME

        # API KEY
        _has_apikey("QDRANT_API_KEY")
        qdrant_api_key = os.environ.get("QDRANT_API_KEY")
        qdrant_kwargs = {}
        qdrant_kwargs['api_key'] = qdrant_api_key

        # Check collection:
        client = QdrantClient(
            url=url,
            api_key=qdrant_api_key,
        )

        try:
            collection_info = client.get_collection(collection_name)
            print(f"  Points (fragments): {collection_info.points_count}")

            if collection_info.points_count == 0:
                print("\nEmpty collection !!")

        except Exception as e:
            print(f"\nERROR getting collection information: {e}")


        # Lazy load with Qdrant.from_documents() if neccessary

        # Load & connect collection
        self.vectorstore = QdrantVectorStore.from_existing_collection(
            embedding=self.embeddings, # Note: keyword 'embeddings' is now: 'embedding'
            collection_name=collection_name,
            url=url,
            **qdrant_kwargs
        )

        print(f"Vectorstore connected to Qdrant Collection: {collection_name}")

    def ask(self, query: str, chat_history: list[tuple[str, str]]) -> str:
        """Invoke RAG with question and chat history."""

        result = self.qa_chain.invoke({"question": query, "chat_history": chat_history})
        return result["answer"]

