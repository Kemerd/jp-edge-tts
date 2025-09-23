#!/usr/bin/env python3
"""
Train DeepPhonemizer model for Japanese G2P conversion

This script trains a DeepPhonemizer model on Japanese text-phoneme pairs
from the ja_phonemes.json dictionary and exports it to ONNX format.

Author: JP Edge TTS Project
Date: 2024
"""

import json
import sys
import os
import argparse
import logging
from pathlib import Path
from typing import List, Tuple, Optional
import random
from collections import defaultdict

# Add DeepPhonemizer to path
sys.path.append(str(Path(__file__).parent.parent / '.github' / 'DeepPhonemizer'))

import torch
import torch.nn as nn
import numpy as np
from tqdm import tqdm

# Import DeepPhonemizer modules
from dp.preprocess import preprocess
from dp.train import train
from dp.phonemizer import Phonemizer
from dp.model.model import AutoregressiveTransformer, ForwardTransformer
from dp.model.predictor import Predictor
from dp.model.utils import get_config_from_file

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class JapaneseDataProcessor:
    """
    Process Japanese text-phoneme pairs for training
    """

    def __init__(self, dictionary_path: str, lexicon_path: Optional[str] = None):
        """
        Initialize the data processor

        Args:
            dictionary_path: Path to ja_phonemes.json
            lexicon_path: Optional path to additional lexicon
        """
        self.dictionary_path = dictionary_path
        self.lexicon_path = lexicon_path
        self.data = []
        self.char_set = set()
        self.phoneme_set = set()

    def load_data(self) -> List[Tuple[str, str, str]]:
        """
        Load and process Japanese phoneme dictionary

        Returns:
            List of (language, text, phonemes) tuples
        """
        logger.info(f"Loading dictionary from {self.dictionary_path}")

        # Load main dictionary
        with open(self.dictionary_path, 'r', encoding='utf-8') as f:
            phoneme_dict = json.load(f)

        # Load additional lexicon if provided
        if self.lexicon_path and os.path.exists(self.lexicon_path):
            logger.info(f"Loading additional lexicon from {self.lexicon_path}")
            with open(self.lexicon_path, 'r', encoding='utf-8') as f:
                additional = json.load(f)
                phoneme_dict.update(additional)

        # Process entries
        for word, phonemes in phoneme_dict.items():
            # Skip entries that are too short or too long
            if len(word) < 1 or len(word) > 50:
                continue

            # Clean phoneme string
            phonemes_clean = self._clean_phonemes(phonemes)

            # Add to dataset
            self.data.append(('ja', word, phonemes_clean))

            # Collect character and phoneme sets
            self.char_set.update(word)
            self.phoneme_set.update(phonemes_clean.split())

        logger.info(f"Loaded {len(self.data)} text-phoneme pairs")
        logger.info(f"Unique characters: {len(self.char_set)}")
        logger.info(f"Unique phonemes: {len(self.phoneme_set)}")

        return self.data

    def _clean_phonemes(self, phonemes: str) -> str:
        """
        Clean and normalize phoneme string

        Args:
            phonemes: Raw phoneme string

        Returns:
            Cleaned phoneme string
        """
        # Remove extra whitespace
        phonemes = ' '.join(phonemes.split())

        # Remove tone markers if needed (keep for Japanese prosody)
        # phonemes = phonemes.replace('ː', '')  # Keep length markers for Japanese

        return phonemes

    def split_data(self, train_ratio: float = 0.8, val_ratio: float = 0.1) -> Tuple[List, List, List]:
        """
        Split data into train/validation/test sets

        Args:
            train_ratio: Ratio for training set
            val_ratio: Ratio for validation set

        Returns:
            Tuple of (train_data, val_data, test_data)
        """
        # Shuffle data
        random.shuffle(self.data)

        # Calculate split points
        n_train = int(len(self.data) * train_ratio)
        n_val = int(len(self.data) * val_ratio)

        # Split data
        train_data = self.data[:n_train]
        val_data = self.data[n_train:n_train + n_val]
        test_data = self.data[n_train + n_val:]

        logger.info(f"Data split - Train: {len(train_data)}, Val: {len(val_data)}, Test: {len(test_data)}")

        return train_data, val_data, test_data

    def augment_data(self, data: List[Tuple[str, str, str]]) -> List[Tuple[str, str, str]]:
        """
        Augment training data with variations

        Args:
            data: Original data

        Returns:
            Augmented data
        """
        augmented = []

        for lang, text, phonemes in data:
            # Add original
            augmented.append((lang, text, phonemes))

            # Add lowercase version if different
            text_lower = text.lower()
            if text_lower != text:
                augmented.append((lang, text_lower, phonemes))

            # Add with different spacing for compounds
            if ' ' not in text and len(text) > 4:
                # Try to split at likely word boundaries
                # This is simplified - real implementation would use MeCab
                mid = len(text) // 2
                text_spaced = text[:mid] + ' ' + text[mid:]
                augmented.append((lang, text_spaced, phonemes))

        logger.info(f"Augmented data from {len(data)} to {len(augmented)} samples")
        return augmented


def create_config(output_dir: str, model_type: str = 'forward') -> str:
    """
    Create configuration file for training

    Args:
        output_dir: Directory for outputs
        model_type: 'forward' or 'autoregressive'

    Returns:
        Path to config file
    """
    config = {
        'seed': 42,
        'lang': 'ja',  # Japanese
        'text_symbols': [],  # Will be filled by preprocess
        'phoneme_symbols': [],  # Will be filled by preprocess

        # Model architecture
        'model': {
            'type': model_type,
            'd_model': 256,
            'd_fft': 1024,
            'layers': 4,
            'heads': 4,
            'dropout': 0.1,
            'reduction_factor': 1,
            'max_len': 200
        },

        # Training settings
        'training': {
            'batch_size': 32,
            'learning_rate': 0.001,
            'warmup_steps': 4000,
            'epochs': 100,
            'grad_clip': 1.0,
            'accumulation_steps': 1,
            'checkpoint_interval': 5,
            'val_interval': 1000,
            'log_interval': 100,
            'early_stopping_patience': 10
        },

        # Data settings
        'data': {
            'train_data': f'{output_dir}/train_data.txt',
            'val_data': f'{output_dir}/val_data.txt',
            'test_data': f'{output_dir}/test_data.txt',
            'preprocessed_dir': f'{output_dir}/preprocessed',
            'char_repeats': 1,  # For Japanese, usually no character repeats
            'lowercase': False,  # Keep original case for Japanese
            'n_val': 500
        },

        # Paths
        'paths': {
            'checkpoint_dir': f'{output_dir}/checkpoints',
            'log_dir': f'{output_dir}/logs',
            'result_dir': f'{output_dir}/results'
        },

        # Japanese-specific settings
        'japanese': {
            'use_mecab': True,
            'preserve_particles': True,
            'handle_long_vowels': True,
            'stress_patterns': False  # Japanese doesn't have stress like English
        }
    }

    # Save config
    config_path = os.path.join(output_dir, f'{model_type}_config.yaml')

    # Convert to YAML format
    import yaml
    with open(config_path, 'w', encoding='utf-8') as f:
        yaml.dump(config, f, default_flow_style=False, allow_unicode=True)

    logger.info(f"Created config file: {config_path}")
    return config_path


def prepare_data_files(train_data: List, val_data: List, test_data: List, output_dir: str):
    """
    Write data to text files for DeepPhonemizer

    Args:
        train_data: Training samples
        val_data: Validation samples
        test_data: Test samples
        output_dir: Output directory
    """
    os.makedirs(output_dir, exist_ok=True)

    # Write train data
    train_file = os.path.join(output_dir, 'train_data.txt')
    with open(train_file, 'w', encoding='utf-8') as f:
        for lang, text, phonemes in train_data:
            f.write(f"{lang}\t{text}\t{phonemes}\n")
    logger.info(f"Wrote {len(train_data)} training samples to {train_file}")

    # Write validation data
    val_file = os.path.join(output_dir, 'val_data.txt')
    with open(val_file, 'w', encoding='utf-8') as f:
        for lang, text, phonemes in val_data:
            f.write(f"{lang}\t{text}\t{phonemes}\n")
    logger.info(f"Wrote {len(val_data)} validation samples to {val_file}")

    # Write test data
    test_file = os.path.join(output_dir, 'test_data.txt')
    with open(test_file, 'w', encoding='utf-8') as f:
        for lang, text, phonemes in test_data:
            f.write(f"{lang}\t{text}\t{phonemes}\n")
    logger.info(f"Wrote {len(test_data)} test samples to {test_file}")


def train_model(config_file: str, train_data: List, val_data: List, use_gpu: bool = True):
    """
    Train DeepPhonemizer model

    Args:
        config_file: Path to configuration file
        train_data: Training samples
        val_data: Validation samples
        use_gpu: Whether to use GPU
    """
    logger.info("Starting model training...")

    # Preprocess data
    preprocess(
        config_file=config_file,
        train_data=train_data,
        val_data=val_data,
        deduplicate_train_data=True
    )

    # Check GPU availability
    num_gpus = torch.cuda.device_count() if use_gpu else 0
    logger.info(f"Training with {num_gpus} GPU(s)")

    # Train model
    if num_gpus > 1:
        # Multi-GPU training
        import torch.multiprocessing as mp
        mp.spawn(train, nprocs=num_gpus, args=(num_gpus, config_file))
    else:
        # Single GPU or CPU training
        train(rank=0, num_gpus=num_gpus, config_file=config_file)

    logger.info("Training completed!")


def export_to_onnx(checkpoint_path: str, output_path: str, model_type: str = 'forward'):
    """
    Export trained model to ONNX format

    Args:
        checkpoint_path: Path to model checkpoint
        output_path: Output ONNX file path
        model_type: Model type ('forward' or 'autoregressive')
    """
    logger.info(f"Exporting model from {checkpoint_path} to {output_path}")

    # Load checkpoint
    checkpoint = torch.load(checkpoint_path, map_location='cpu')
    config = checkpoint.get('config', {})

    # Create model
    if model_type == 'forward':
        model = ForwardTransformer(
            n_symbols=len(config.get('text_symbols', [])),
            n_phonemes=len(config.get('phoneme_symbols', [])),
            d_model=config['model']['d_model'],
            d_fft=config['model']['d_fft'],
            layers=config['model']['layers'],
            heads=config['model']['heads'],
            dropout=0  # No dropout for inference
        )
    else:
        model = AutoregressiveTransformer(
            n_symbols=len(config.get('text_symbols', [])),
            n_phonemes=len(config.get('phoneme_symbols', [])),
            d_model=config['model']['d_model'],
            d_fft=config['model']['d_fft'],
            layers=config['model']['layers'],
            heads=config['model']['heads'],
            dropout=0
        )

    # Load weights
    model.load_state_dict(checkpoint['model'])
    model.eval()

    # Create dummy input for tracing
    batch_size = 1
    seq_len = 50
    dummy_input = torch.randint(0, 100, (batch_size, seq_len))

    # Export to ONNX
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={
            'input': {0: 'batch_size', 1: 'sequence'},
            'output': {0: 'batch_size', 1: 'sequence'}
        }
    )

    logger.info(f"Model exported to {output_path}")

    # Verify ONNX model
    try:
        import onnx
        onnx_model = onnx.load(output_path)
        onnx.checker.check_model(onnx_model)
        logger.info("ONNX model verification successful!")
    except ImportError:
        logger.warning("ONNX not installed, skipping verification")
    except Exception as e:
        logger.error(f"ONNX model verification failed: {e}")


def main():
    """Main training pipeline"""
    parser = argparse.ArgumentParser(description='Train DeepPhonemizer for Japanese G2P')

    parser.add_argument(
        '--dictionary',
        type=str,
        default='data/ja_phonemes.json',
        help='Path to Japanese phoneme dictionary'
    )
    parser.add_argument(
        '--lexicon',
        type=str,
        help='Path to additional lexicon file'
    )
    parser.add_argument(
        '--output-dir',
        type=str,
        default='training_output',
        help='Output directory for models and logs'
    )
    parser.add_argument(
        '--model-type',
        type=str,
        choices=['forward', 'autoregressive'],
        default='forward',
        help='Model type to train'
    )
    parser.add_argument(
        '--epochs',
        type=int,
        default=50,
        help='Number of training epochs'
    )
    parser.add_argument(
        '--batch-size',
        type=int,
        default=32,
        help='Training batch size'
    )
    parser.add_argument(
        '--learning-rate',
        type=float,
        default=0.001,
        help='Learning rate'
    )
    parser.add_argument(
        '--augment',
        action='store_true',
        help='Augment training data'
    )
    parser.add_argument(
        '--export-onnx',
        action='store_true',
        help='Export model to ONNX after training'
    )
    parser.add_argument(
        '--use-gpu',
        action='store_true',
        default=torch.cuda.is_available(),
        help='Use GPU for training'
    )

    args = parser.parse_args()

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # Load and process data
    processor = JapaneseDataProcessor(args.dictionary, args.lexicon)
    data = processor.load_data()

    # Split data
    train_data, val_data, test_data = processor.split_data()

    # Augment if requested
    if args.augment:
        train_data = processor.augment_data(train_data)

    # Prepare data files
    prepare_data_files(train_data, val_data, test_data, args.output_dir)

    # Create configuration
    config_file = create_config(args.output_dir, args.model_type)

    # Train model
    train_model(config_file, train_data, val_data, args.use_gpu)

    # Export to ONNX if requested
    if args.export_onnx:
        checkpoint_path = os.path.join(
            args.output_dir,
            'checkpoints',
            'best_model.pt'
        )
        onnx_path = os.path.join(args.output_dir, 'phonemizer.onnx')

        if os.path.exists(checkpoint_path):
            export_to_onnx(checkpoint_path, onnx_path, args.model_type)
        else:
            logger.warning(f"Checkpoint not found at {checkpoint_path}")

    logger.info("Training pipeline completed!")


if __name__ == '__main__':
    main()