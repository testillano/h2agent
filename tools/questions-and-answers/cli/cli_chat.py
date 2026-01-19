# cli/cli_chat.py

# To found core module (if not, we should run this way: python3 -m cli.cli_chat)
import sys
import os
# __file__ -> 'cli/cli_chat.py'
# os.path.dirname(os.path.abspath(__file__)) -> 'cli'
# os.path.join(..., os.pardir) -> root
script_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.join(script_dir, os.pardir)
sys.path.insert(0, parent_dir)

import pickle
from core.rag_service import RAGService

CHAT_HISTORY_FILE = "chat-history.pkl"

def run_cli_chat():
    rag_service = RAGService(vectorstore_path="./vectorstore-data")

    # Load history:
    chat_history = []
    try:
        with open(CHAT_HISTORY_FILE, 'rb') as f:
            chat_history = pickle.load(f)
            print(f"Chat history loaded: {len(chat_history)} interactions.")
    except:
        print("New chat history initiated.")
        pass

    # Bucle principal de la CLI
    print("\n--- H2AGENT RAG Console Chat ---")
    while True:
        query = input("\nAsk me anything (0 = quit): ")
        if query == "0":
            break

        # 1. Obtener respuesta del RAG Service
        answer = rag_service.ask(query, chat_history)

        # 2. Imprimir respuesta y actualizar historial
        print(answer)
        chat_history.append((query, answer))

    # Guardar el historial antes de salir
    print(f"\n[saving chat history ({len(chat_history)} interactions) ...]\n")
    with open(CHAT_HISTORY_FILE, 'wb') as f:
        pickle.dump(chat_history, f)

    print("Done !")

if __name__ == "__main__":
    run_cli_chat()
