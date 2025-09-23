"""
JP Edge TTS - Phonemizer Training Interface
Streamlit-powered training application for DeepPhonemizer models
"""

import streamlit as st
import tensorflow as tf
import numpy as np
import pandas as pd
import json
import os
from pathlib import Path
from datetime import datetime
import matplotlib.pyplot as plt
from tqdm import tqdm
import sounddevice as sd
import librosa
import scipy.signal
import resampy
from typing import Dict, List, Tuple, Optional, Union
import pickle
import io
import base64

# Page configuration - must be first Streamlit command
st.set_page_config(
    page_title="JP Edge TTS Training",
    page_icon="🎌",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Custom CSS for Apple-like design
st.markdown("""
<style>
    /* Modern Apple-style design with smooth animations */
    .stApp {
        background: #2d2d30;
    }
    .main-header {
        font-size: 3rem;
        font-weight: 700;
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        -webkit-background-clip: text;
        -webkit-text-fill-color: transparent;
        text-align: center;
        margin-bottom: 2rem;
    }
    .metric-card {
        background: rgba(255, 255, 255, 0.95);
        backdrop-filter: blur(10px);
        border-radius: 20px;
        padding: 1.5rem;
        box-shadow: 0 8px 32px rgba(0,0,0,0.1);
        transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .metric-card:hover {
        transform: translateY(-4px);
        box-shadow: 0 12px 48px rgba(0,0,0,0.15);
    }
    .stButton button {
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        border: none;
        border-radius: 12px;
        padding: 0.75rem 2rem;
        font-weight: 600;
        transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .stButton button:hover {
        transform: scale(1.05);
        box-shadow: 0 8px 24px rgba(102, 126, 234, 0.4);
    }
    /* Remove tab button backgrounds */
    .stTabs [data-baseweb="tab-list"] button {
        background: transparent !important;
        border: none !important;
        color: #ffffff !important;
    }
    .stTabs [data-baseweb="tab-list"] button[aria-selected="true"] {
        background: transparent !important;
        border-bottom: 2px solid #667eea !important;
        color: #667eea !important;
    }
    .training-progress {
        background: rgba(255, 255, 255, 0.95);
        border-radius: 16px;
        padding: 1.5rem;
        margin: 1rem 0;
    }
</style>
""", unsafe_allow_html=True)

class DeepPhonemizer:
    """
    DeepPhonemizer model for grapheme-to-phoneme conversion
    Supports both dictionary lookup and neural network approaches
    """

    def __init__(self, model_type: str = "transformer"):
        # Model configuration based on type
        self.model_type = model_type
        self.model = None
        self.tokenizer = None
        self.config = {
            "max_seq_length": 128,
            "embedding_dim": 256,
            "num_heads": 8,
            "num_layers": 4,
            "dropout_rate": 0.1,
            "vocab_size": 10000
        }

    def build_transformer_model(self, vocab_size: int, phoneme_size: int):
        """
        Build a transformer-based G2P model using TensorFlow
        """
        # Input layers
        inputs = tf.keras.Input(shape=(None,), dtype=tf.int32, name="text_input")

        # Token and positional embeddings
        embedding_layer = tf.keras.layers.Embedding(
            vocab_size,
            self.config["embedding_dim"],
            name="token_embedding"
        )(inputs)

        # Add positional encoding
        positions = tf.range(start=0, limit=tf.shape(embedding_layer)[1], delta=1)
        position_embeddings = tf.keras.layers.Embedding(
            self.config["max_seq_length"],
            self.config["embedding_dim"],
            name="position_embedding"
        )(positions)

        embeddings = embedding_layer + position_embeddings
        embeddings = tf.keras.layers.Dropout(self.config["dropout_rate"])(embeddings)

        # Transformer encoder blocks
        x = embeddings
        for i in range(self.config["num_layers"]):
            # Multi-head attention
            attention_output = tf.keras.layers.MultiHeadAttention(
                num_heads=self.config["num_heads"],
                key_dim=self.config["embedding_dim"] // self.config["num_heads"],
                name=f"attention_{i}"
            )(x, x)
            attention_output = tf.keras.layers.Dropout(self.config["dropout_rate"])(attention_output)
            x = tf.keras.layers.LayerNormalization(epsilon=1e-6)(x + attention_output)

            # Feed-forward network
            ffn_output = tf.keras.layers.Dense(
                self.config["embedding_dim"] * 4,
                activation="relu",
                name=f"ffn_expand_{i}"
            )(x)
            ffn_output = tf.keras.layers.Dense(
                self.config["embedding_dim"],
                name=f"ffn_contract_{i}"
            )(ffn_output)
            ffn_output = tf.keras.layers.Dropout(self.config["dropout_rate"])(ffn_output)
            x = tf.keras.layers.LayerNormalization(epsilon=1e-6)(x + ffn_output)

        # Output layer for phoneme prediction
        outputs = tf.keras.layers.Dense(phoneme_size, activation="softmax", name="phoneme_output")(x)

        # Create model
        model = tf.keras.Model(inputs=inputs, outputs=outputs, name="deep_phonemizer")
        return model

    def build_lstm_model(self, vocab_size: int, phoneme_size: int):
        """
        Build an LSTM-based G2P model as an alternative
        """
        model = tf.keras.Sequential([
            tf.keras.layers.Embedding(vocab_size, self.config["embedding_dim"]),
            tf.keras.layers.Bidirectional(
                tf.keras.layers.LSTM(256, return_sequences=True, dropout=0.2)
            ),
            tf.keras.layers.Bidirectional(
                tf.keras.layers.LSTM(128, return_sequences=True, dropout=0.2)
            ),
            tf.keras.layers.Dense(phoneme_size, activation="softmax")
        ], name="lstm_phonemizer")
        return model

    def compile_model(self, learning_rate: float = 0.001):
        """
        Compile the model with optimizer and loss function
        """
        optimizer = tf.keras.optimizers.Adam(learning_rate=learning_rate)
        self.model.compile(
            optimizer=optimizer,
            loss="sparse_categorical_crossentropy",
            metrics=["accuracy"]
        )

    def export_to_onnx(self, output_path: str):
        """
        Export the trained model to ONNX format for inference
        """
        try:
            import tf2onnx

            # Define input signature
            spec = (tf.TensorSpec((None, None), tf.int32, name="input"),)

            # Convert to ONNX
            model_proto, _ = tf2onnx.convert.from_keras(
                self.model,
                input_signature=spec,
                opset=13,
                output_path=output_path
            )

            return True
        except ImportError:
            st.error("tf2onnx not installed. Install with: pip install tf2onnx")
            return False

class DatasetManager:
    """
    Manages training data for the phonemizer
    """

    @staticmethod
    def load_phoneme_dictionary(file_path: str) -> Dict[str, str]:
        """
        Load Japanese phoneme dictionary from JSON
        """
        with open(file_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    @staticmethod
    def prepare_training_data(
        dictionary: Dict[str, str],
        test_split: float = 0.1,
        val_split: float = 0.1
    ) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """
        Prepare training, validation, and test datasets from dictionary
        """
        # Convert dictionary to lists
        words = list(dictionary.keys())
        phonemes = list(dictionary.values())

        # Create character and phoneme vocabularies
        char_vocab = sorted(set(''.join(words)))
        phoneme_vocab = sorted(set(' '.join(phonemes).split()))

        # Create mappings
        char_to_idx = {char: idx + 1 for idx, char in enumerate(char_vocab)}  # 0 reserved for padding
        phoneme_to_idx = {ph: idx + 1 for idx, ph in enumerate(phoneme_vocab)}

        # Encode data
        X = []
        y = []

        for word, phoneme_seq in zip(words, phonemes):
            # Encode word
            word_encoded = [char_to_idx.get(char, 0) for char in word]
            # Encode phonemes
            phoneme_encoded = [phoneme_to_idx.get(ph, 0) for ph in phoneme_seq.split()]

            X.append(word_encoded)
            y.append(phoneme_encoded)

        # Pad sequences
        max_word_len = max(len(seq) for seq in X)
        max_phoneme_len = max(len(seq) for seq in y)

        X = tf.keras.preprocessing.sequence.pad_sequences(X, maxlen=max_word_len, padding='post')
        y = tf.keras.preprocessing.sequence.pad_sequences(y, maxlen=max_phoneme_len, padding='post')

        # Split data
        n_samples = len(X)
        n_test = int(n_samples * test_split)
        n_val = int(n_samples * val_split)
        n_train = n_samples - n_test - n_val

        # Shuffle and split
        indices = np.random.permutation(n_samples)

        train_idx = indices[:n_train]
        val_idx = indices[n_train:n_train + n_val]
        test_idx = indices[n_train + n_val:]

        X_train, y_train = X[train_idx], y[train_idx]
        X_val, y_val = X[val_idx], y[val_idx]
        X_test, y_test = X[test_idx], y[test_idx]

        return X_train, y_train, X_val, y_val, X_test, y_test

# Initialize session state
if 'model' not in st.session_state:
    st.session_state.model = None
if 'training_history' not in st.session_state:
    st.session_state.training_history = None
if 'is_training' not in st.session_state:
    st.session_state.is_training = False

def main():
    """
    Main Streamlit application
    """

    # Header with gradient text
    st.markdown('<h1 class="main-header">🎌 JP Edge TTS Training Studio</h1>', unsafe_allow_html=True)
    st.markdown("### Train custom Japanese phonemizer models with an intuitive interface")

    # Sidebar configuration
    with st.sidebar:
        st.markdown("## 🎛️ Configuration")

        # Model selection
        model_type = st.selectbox(
            "Model Architecture",
            ["Transformer", "LSTM", "Dictionary-based"],
            help="Choose the model architecture for phonemization"
        )

        # Training parameters
        st.markdown("### Training Parameters")
        epochs = st.slider("Epochs", 1, 200, 50, help="Number of training epochs")
        batch_size = st.slider("Batch Size", 8, 128, 32, step=8, help="Training batch size")
        learning_rate = st.number_input(
            "Learning Rate",
            min_value=0.00001,
            max_value=0.1,
            value=0.001,
            format="%.5f",
            help="Optimizer learning rate"
        )

        # Data split configuration
        st.markdown("### Data Split")
        val_split = st.slider("Validation %", 0, 30, 10, help="Percentage for validation")
        test_split = st.slider("Test %", 0, 30, 10, help="Percentage for testing")

        # Export options
        st.markdown("### Export Options")
        export_onnx = st.checkbox("Export to ONNX", value=True, help="Export model to ONNX format after training")
        export_tflite = st.checkbox("Export to TFLite", help="Export model to TensorFlow Lite format")

    # Main content area with tabs
    tab1, tab2, tab3, tab4 = st.tabs(["📊 Dataset", "🚀 Training", "📈 Metrics", "🔍 Testing"])

    with tab1:
        st.markdown("### Dataset Management")

        col1, col2 = st.columns(2)

        with col1:
            # Upload phoneme dictionary
            uploaded_file = st.file_uploader(
                "Upload Phoneme Dictionary (JSON)",
                type=['json'],
                help="JSON file with word-to-phoneme mappings"
            )

            if uploaded_file:
                dictionary = json.load(uploaded_file)
                st.success(f"✅ Loaded {len(dictionary)} entries")

                # Display sample entries
                st.markdown("#### Sample Entries")
                sample_df = pd.DataFrame(
                    list(dictionary.items())[:5],
                    columns=["Word", "Phonemes"]
                )
                st.dataframe(sample_df, use_container_width=True)

        with col2:
            # Dataset statistics
            if uploaded_file:
                st.markdown("#### Dataset Statistics")

                # Calculate statistics with nice cards
                total_words = len(dictionary)
                unique_chars = len(set(''.join(dictionary.keys())))
                unique_phonemes = len(set(' '.join(dictionary.values()).split()))

                # Display metrics in cards
                metric_col1, metric_col2, metric_col3 = st.columns(3)

                with metric_col1:
                    st.metric("Total Words", f"{total_words:,}")

                with metric_col2:
                    st.metric("Unique Characters", unique_chars)

                with metric_col3:
                    st.metric("Unique Phonemes", unique_phonemes)

                # Distribution plot
                st.markdown("#### Length Distribution")

                word_lengths = [len(word) for word in dictionary.keys()]
                phoneme_lengths = [len(ph.split()) for ph in dictionary.values()]

                fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))

                # Word length distribution
                ax1.hist(word_lengths, bins=20, alpha=0.7, color='#667eea', edgecolor='black')
                ax1.set_xlabel("Word Length (characters)")
                ax1.set_ylabel("Frequency")
                ax1.set_title("Word Length Distribution")
                ax1.grid(True, alpha=0.3)

                # Phoneme length distribution
                ax2.hist(phoneme_lengths, bins=20, alpha=0.7, color='#764ba2', edgecolor='black')
                ax2.set_xlabel("Phoneme Sequence Length")
                ax2.set_ylabel("Frequency")
                ax2.set_title("Phoneme Length Distribution")
                ax2.grid(True, alpha=0.3)

                st.pyplot(fig)

    with tab2:
        st.markdown("### Model Training")

        if uploaded_file is None:
            st.warning("⚠️ Please upload a phoneme dictionary first")
        else:
            col1, col2, col3 = st.columns([1, 2, 1])

            with col2:
                # Training button with animation
                if st.button("🚀 Start Training", use_container_width=True, disabled=st.session_state.is_training):
                    st.session_state.is_training = True

                    # Prepare data
                    with st.spinner("Preparing dataset..."):
                        dataset_manager = DatasetManager()
                        X_train, y_train, X_val, y_val, X_test, y_test = dataset_manager.prepare_training_data(
                            dictionary,
                            test_split=test_split/100,
                            val_split=val_split/100
                        )

                    # Calculate vocabulary sizes
                    vocab_size = len(set(''.join(dictionary.keys()))) + 1  # +1 for padding
                    phoneme_size = len(set(' '.join(dictionary.values()).split())) + 1

                    # Build model
                    with st.spinner(f"Building {model_type} model..."):
                        phonemizer = DeepPhonemizer(model_type.lower())

                        if model_type == "Transformer":
                            phonemizer.model = phonemizer.build_transformer_model(vocab_size, phoneme_size)
                        elif model_type == "LSTM":
                            phonemizer.model = phonemizer.build_lstm_model(vocab_size, phoneme_size)

                        phonemizer.compile_model(learning_rate)
                        st.session_state.model = phonemizer

                    # Display model summary
                    model_summary = []
                    phonemizer.model.summary(print_fn=lambda x: model_summary.append(x))

                    with st.expander("Model Architecture"):
                        st.text('\n'.join(model_summary))

                    # Training progress container
                    progress_container = st.container()

                    with progress_container:
                        st.markdown('<div class="training-progress">', unsafe_allow_html=True)

                        # Progress bar
                        progress_bar = st.progress(0)
                        status_text = st.empty()

                        # Metrics display during training
                        metric_cols = st.columns(4)
                        epoch_metric = metric_cols[0].empty()
                        loss_metric = metric_cols[1].empty()
                        acc_metric = metric_cols[2].empty()
                        val_loss_metric = metric_cols[3].empty()

                        # Training loop with live updates
                        history = {'loss': [], 'accuracy': [], 'val_loss': [], 'val_accuracy': []}

                        for epoch in range(epochs):
                            # Update progress
                            progress = (epoch + 1) / epochs
                            progress_bar.progress(progress)
                            status_text.text(f"Training epoch {epoch + 1}/{epochs}")

                            # Train for one epoch
                            hist = phonemizer.model.fit(
                                X_train, y_train,
                                batch_size=batch_size,
                                epochs=1,
                                validation_data=(X_val, y_val),
                                verbose=0
                            )

                            # Update history
                            for key in history:
                                if key in hist.history:
                                    history[key].append(hist.history[key][0])

                            # Update live metrics with smooth animation
                            epoch_metric.metric("Epoch", f"{epoch + 1}/{epochs}")
                            loss_metric.metric("Loss", f"{history['loss'][-1]:.4f}")
                            acc_metric.metric("Accuracy", f"{history['accuracy'][-1]:.2%}")
                            val_loss_metric.metric("Val Loss", f"{history['val_loss'][-1]:.4f}")

                        st.markdown('</div>', unsafe_allow_html=True)

                        # Save training history
                        st.session_state.training_history = history
                        st.session_state.is_training = False

                        st.success("✅ Training completed successfully!")

                        # Export options
                        if export_onnx:
                            with st.spinner("Exporting to ONNX..."):
                                output_path = f"phonemizer_{model_type.lower()}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.onnx"
                                if phonemizer.export_to_onnx(output_path):
                                    st.success(f"📦 Model exported to {output_path}")

                        # Save model button
                        col1, col2 = st.columns(2)
                        with col1:
                            if st.button("💾 Save TensorFlow Model"):
                                save_path = f"phonemizer_{model_type.lower()}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
                                phonemizer.model.save(save_path)
                                st.success(f"Model saved to {save_path}")

                        with col2:
                            if st.button("📥 Download Model Weights"):
                                # Serialize model weights
                                weights = phonemizer.model.get_weights()
                                buffer = io.BytesIO()
                                pickle.dump(weights, buffer)
                                buffer.seek(0)

                                # Create download button
                                st.download_button(
                                    label="Download Weights",
                                    data=buffer.getvalue(),
                                    file_name=f"weights_{model_type.lower()}.pkl",
                                    mime="application/octet-stream"
                                )

    with tab3:
        st.markdown("### Training Metrics")

        if st.session_state.training_history:
            history = st.session_state.training_history

            # Create beautiful metric plots with Apple-style design
            fig, axes = plt.subplots(1, 2, figsize=(14, 5))

            # Loss plot
            axes[0].plot(history['loss'], label='Training Loss', color='#667eea', linewidth=2)
            axes[0].plot(history['val_loss'], label='Validation Loss', color='#764ba2', linewidth=2)
            axes[0].set_xlabel('Epoch')
            axes[0].set_ylabel('Loss')
            axes[0].set_title('Model Loss', fontsize=14, fontweight='bold')
            axes[0].legend(loc='upper right')
            axes[0].grid(True, alpha=0.3)
            axes[0].set_facecolor('#f8f9fa')

            # Accuracy plot
            axes[1].plot(history['accuracy'], label='Training Accuracy', color='#667eea', linewidth=2)
            axes[1].plot(history['val_accuracy'], label='Validation Accuracy', color='#764ba2', linewidth=2)
            axes[1].set_xlabel('Epoch')
            axes[1].set_ylabel('Accuracy')
            axes[1].set_title('Model Accuracy', fontsize=14, fontweight='bold')
            axes[1].legend(loc='lower right')
            axes[1].grid(True, alpha=0.3)
            axes[1].set_facecolor('#f8f9fa')

            plt.tight_layout()
            st.pyplot(fig)

            # Display final metrics
            st.markdown("### Final Performance")
            col1, col2, col3, col4 = st.columns(4)

            with col1:
                st.metric(
                    "Final Training Loss",
                    f"{history['loss'][-1]:.4f}",
                    delta=f"{history['loss'][-1] - history['loss'][0]:.4f}"
                )

            with col2:
                st.metric(
                    "Final Training Accuracy",
                    f"{history['accuracy'][-1]:.2%}",
                    delta=f"{(history['accuracy'][-1] - history['accuracy'][0])*100:.1f}%"
                )

            with col3:
                st.metric(
                    "Final Validation Loss",
                    f"{history['val_loss'][-1]:.4f}",
                    delta=f"{history['val_loss'][-1] - history['val_loss'][0]:.4f}"
                )

            with col4:
                st.metric(
                    "Final Validation Accuracy",
                    f"{history['val_accuracy'][-1]:.2%}",
                    delta=f"{(history['val_accuracy'][-1] - history['val_accuracy'][0])*100:.1f}%"
                )
        else:
            st.info("📊 Train a model to see metrics")

    with tab4:
        st.markdown("### Model Testing")

        if st.session_state.model:
            # Interactive testing interface
            test_text = st.text_input(
                "Enter Japanese text to test",
                placeholder="こんにちは",
                help="Enter text to convert to phonemes"
            )

            if test_text and st.button("🔍 Convert to Phonemes"):
                # Here you would implement the actual inference
                # For now, showing a placeholder
                st.success(f"Phonemes: k o n n i ch i w a")

                # Show confidence scores with a nice visualization
                st.markdown("#### Confidence Scores")
                # Placeholder confidence data
                confidences = np.random.rand(len(test_text))

                fig, ax = plt.subplots(figsize=(10, 3))
                bars = ax.bar(range(len(test_text)), confidences, color='#667eea', alpha=0.8)
                ax.set_xticks(range(len(test_text)))
                ax.set_xticklabels(list(test_text))
                ax.set_ylabel('Confidence')
                ax.set_ylim(0, 1)
                ax.grid(True, alpha=0.3, axis='y')

                # Color bars based on confidence
                for bar, conf in zip(bars, confidences):
                    if conf < 0.5:
                        bar.set_color('#ff6b6b')
                    elif conf < 0.8:
                        bar.set_color('#ffd93d')
                    else:
                        bar.set_color('#6bcf7f')

                st.pyplot(fig)
        else:
            st.info("🚀 Train a model first to test it")

    # Footer with additional information
    st.markdown("---")
    st.markdown(
        """
        <div style='text-align: center; color: #666;'>
        <p>JP Edge TTS Training Studio v1.0 | Built with ❤️ for Japanese language processing</p>
        </div>
        """,
        unsafe_allow_html=True
    )

if __name__ == "__main__":
    main()