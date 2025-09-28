################################################
# QUESTIONS AND ANSWERS FROM PROJECT MARKDOWNS #
################################################

# Imports
import os, sys, glob, pickle

from langchain_community.document_loaders import UnstructuredMarkdownLoader # pip3 install --upgrade requests
from langchain.text_splitter import MarkdownTextSplitter
#from langchain_community.document_loaders import PyPDFLoader
#from langchain.text_splitter import CharacterTextSplitter
from langchain_openai import OpenAIEmbeddings
from langchain_openai import OpenAI
from langchain_groq import ChatGroq

from langchain.chains import ConversationalRetrievalChain # RetrievalQA is more simple (no context)

from langchain_chroma import Chroma

# Uncomment for debug purposes:
#from langchain.globals import set_verbose
#set_verbose(True)


# Script location
SCR_PATH = os.path.abspath(__file__)
SCR_DIR, SCR_BN = os.path.split(SCR_PATH)
TARGET_DIR = os.path.abspath(SCR_DIR + "/../..")

# Chat history
CHAT_HISTORY = "chat-history"

# Basic checkings:
# Python3 version
major = sys.version_info.major
minor = sys.version_info.minor
micro = sys.version_info.micro
if major < 3 or (major == 3 and minor < 8):
  print("Python version must be >= 3.8.1 (current: {x}.{y}.{z}). Try alias it, i.e.: alias python3='/usr/bin/python3.9'".format(x=major, y=minor, z=micro))
  sys.exit(1)

# API KEYs:
def has_apikey(key, mandatory = True):
  result = False
  try:
    dummy = os.environ[key]
    result = True
  except:
    if mandatory:
      print("ERROR: you must define '{}' environment variable".format(key))
      sys.exit(1)
    else:
      print("WARNING: you may want to define '{}' environment variable".format(key))

  return result

# Check OpenAI api key:
has_apikey("OPENAI_API_KEY")

# Check optional Groq api key:
has_groq = has_apikey("GROQ_API_KEY", False)

# Select which enbeddings we want to use
embeddings = OpenAIEmbeddings()

# Load documents into RAG: read if exsits
vectorstore_path = "./vectorstore-data"
if not os.path.exists(vectorstore_path):
  print("Creating vectorstore ...")

  wildcard= TARGET_DIR + '/**/*.md' # or '/**/*.pdf' for PDF files
  all_files = glob.glob(wildcard, recursive = True)

  # Exclude files I don't want in the RAG:
  exclude_patterns = [
    'pull_request_template.md',
    'AggregatedKeys.md',
    'tools/questions-and-answers/README.md',
  ]
  # Not needed: glob don't catch hidden files: '/.github/ISSUE_TEMPLATE/', # exclude all within this directory
  files = [
    f for f in all_files
    if not any(pattern in f for pattern in exclude_patterns)
  ]
  print(f"Files ({len(files)}):")
  for file in files:
    print(file)

  print("\nWait while embeddings are created ...")

  loaders = [UnstructuredMarkdownLoader(os.path.join(SCR_DIR, f)) for f in files]
  #loaders.append(UnstructuredURLLoader(["https://prezi.com/p/1ijxuu0rt-sj/?present=1)"])) # For URLs
  #loaders = [PyPDFLoader(os.path.join(SCR_DIR, f)) for f in files] # For PDF files

  documents = []
  for loader in loaders:
    documents.extend(loader.load())

  # Split the document into chunks:
  text_splitter = MarkdownTextSplitter(chunk_size=2500, chunk_overlap=250)
  fragments = text_splitter.split_documents(documents)
  # For PDF files:
  #text_splitter = CharacterTextSplitter(separator="\n", chunk_size=1000, chunk_overlap=150, length_function=len)
  #fragments = text_splitter.split_documents(documents)

  # Create the vectorstore to use as the index
  vectorstore = Chroma.from_documents(fragments, embeddings, persist_directory=vectorstore_path)

else:
    print("Loading vectorstore ...")
    vectorstore = Chroma(persist_directory=vectorstore_path, embedding_function=embeddings)

print(f"Vectorstore documents: {vectorstore._collection.count()}")

# Expose vectorstore index in a retriever interface
retriever = vectorstore.as_retriever(search_type="similarity", search_kwargs={"k":9})

# Create a chain to answer questions (temperature 0 to be more deterministic, less creative)
llm = OpenAI(temperature=0)

if has_groq:
  llm=ChatGroq(
    model="llama-3.3-70b-versatile",
    temperature=0,
    max_tokens=None,
    timeout=None,
    max_retries=2,
    # other params...
  )

qa = ConversationalRetrievalChain.from_llm(llm, retriever)

# Chat history: read if exists
chat_history = []
try:
  with open(CHAT_HISTORY, 'rb') as f: # open in binary mode
    chat_history = pickle.load(f) # Deserialize the array from the file
except:
  pass

# Main logic
while True:
  query = input("\nAsk me anything (0 = quit): ")
  if query == "0": break
  result = qa.invoke({"question": query, "chat_history": chat_history})
  answer = result["answer"]
  print(answer)
  chat_history.append((query, answer))

# TODO: chat history rotation is needed to avoid huge data and so, greater cost:
print("\n[saving chat history for next chatbot sessions ...]\n")
with open(CHAT_HISTORY, 'wb') as f: # write in binary mode
  pickle.dump(chat_history, f) # serialize the array and write it to the file

print("Done !")
