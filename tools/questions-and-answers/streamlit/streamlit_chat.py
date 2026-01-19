# streamlit/streamlit_chat.py

# To found core module (if not, we should run this way: python3 -m streamlit.streamlit_chat)
import sys
import os
# __file__ -> 'streamlit/streamlit_chat.py'
# os.path.dirname(os.path.abspath(__file__)) -> 'streamlit'
# os.path.join(..., os.pardir) -> root
script_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.join(script_dir, os.pardir)
sys.path.insert(0, parent_dir)


import streamlit as st
from core.rag_service import RAGService

st.set_page_config(page_title="RAG Chatbot", layout="wide")

@st.cache_resource
def get_rag_service():
    return RAGService(vectorstore_path="../vectorstore-data")

rag_service = get_rag_service()

st.title("📚 H2AGENT RAG Server Chat (Streamlit)")

if "chat_history" not in st.session_state:
    st.session_state.chat_history = []

# Show previous history
for query, answer in st.session_state.chat_history:
    with st.chat_message("user"):
        st.markdown(query)
    with st.chat_message("assistant"):
        st.markdown(answer)

if query := st.chat_input("Ask me anything"):
    with st.chat_message("user"):
        st.markdown(query)

    with st.chat_message("assistant"):
        with st.spinner("Wait ..."):
            answer = rag_service.ask(query, st.session_state.chat_history)
            st.markdown(answer)

    # Update history
    st.session_state.chat_history.append((query, answer))
