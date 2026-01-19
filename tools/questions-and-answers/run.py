################################################
# QUESTIONS AND ANSWERS FROM PROJECT MARKDOWNS #
################################################
# QDRANT_URL could be defined to interact qdrant database (try: source source-me.bash)

import sys
import argparse
import os

# Ensure 'from core.rag_service' is resolved
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def main():
    parser = argparse.ArgumentParser(description="RAG Chatbot Multi-Interface Runner.")
    parser.add_argument(
        "--mode",
        type=str,
        choices=["console", "server"],
        default="console",
        help="Select interaction mode: 'console' (CLI) or 'server' (Streamlit)."
    )
    args = parser.parse_args()

    if args.mode == "console":
        print("Initiating RAG Chatbot in console mode (CLI)...")
        from cli.cli_chat import run_cli_chat
        run_cli_chat()

    elif args.mode == "server":
        print("Initiating RAG Chatbot in server mode (Streamlit)...")
        try:
            streamlit_script_path = os.path.join(os.path.dirname(__file__), "streamlit", "streamlit_chat.py")
            os.system(f"streamlit run {streamlit_script_path} --server.headless true")

        except FileNotFoundError:
            print("\nERROR: 'streamlit' command not found.")
            sys.exit(1)


if __name__ == "__main__":
    main()

