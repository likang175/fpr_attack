"""Distinguish attack."""

import numpy as np
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import classification_report
from sklearn.model_selection import StratifiedShuffleSplit

import torch
import torch.nn as nn
from torchinfo import summary
from torch.utils.data import TensorDataset, DataLoader


class ModelCheckpoint:
    def __init__(self,
                 filepath,
                 monitor='val_loss',
                 patience=np.inf):
        self.filepath = filepath
        self.early_stop = False
        self.patience = patience
        self.counter = 0
        self.best_loss = np.inf

    def __call__(self, val_loss, model):
        if val_loss < self.best_loss:
            torch.save({'model': model.state_dict()}, 'model.pth')
            self.best_loss = val_loss
            print("Best model -> saved")
            self.counter = 0
        else:
            self.counter += 1
            # model_save_state = ""
            if self.counter >= self.patience:
                print("Early stopping triggered")
                self.early_stop = True


class EarlyStopping:
    def __init__(self, patience=8):
        self.patience = patience
        self.best = float('inf')
        self.count = 0
        self.best_state = None

    def step(self, val_loss, model):
        if val_loss < self.best:
            self.best = val_loss
            self.count = 0
            self.best_state = {k: v.cpu().clone()
                               for k, v in model.state_dict().items()}
            return False
        else:
            self.count += 1
            return self.count > self.patience

    def restore(self, model):
        if self.best_state is not None:
            model.load_state_dict(self.best_state)


def preprocessing(X0, y0, scaler):
    """Pre-process."""
    N = X0.shape[0]
    X = scaler.transform(X0)
    y = y0
    N_sample = X0.shape[-1]  # input size

    # Add 'channel [C]' dimension: [N, L, C]
    X = X.reshape(N, N_sample, -1)

    # Swap 'L' and 'C' axis to cope with PyTorch convention [N, C, L]
    X = X.transpose(0, 2, 1)

    # Convert to tensors
    X = torch.tensor(X, dtype=torch.float32)
    y = torch.tensor(y, dtype=torch.long)
    return TensorDataset(X, y)


def classifier_train(X_train, y_train, clip_range, model,
                     num_epochs=50,
                     batch_size=128,
                     learning_rate=1e-3,
                     weight_decay=0,
                     val_split=0.1,
                     seed=42):
    """
    Classifier all in one.

    Parameters
    ----------
    X_train : np.ndarray
    Check the success rate and error guess of the template attack.

    Parameters
    ----------
    X_train : np.ndarray
        Shape (#train, #sample), the training set.
    X_test : np.ndarray
        Shape (#test, #sample), the test set.
    y_train : np.ndarray
        Shape (#train,), the training label.
    y_test : np.ndarray
        Shape (#test,), the test label.

    """
    def run_epoch(loader, train_mode=False):
        model.train(train_mode)
        total_loss, N_correct, N_count = 0.0, 0, 0
        y_pred, y_true = [], []
        for xb, yb in loader:
            # xb: x-batch, shape [B, C, L]
            # yb: y-batch, shape [B]
            xb, yb = xb.to(device), yb.to(device)

            if train_mode:
                optimizer.zero_grad()
            logits = model(xb)                 # [B, N_class]
            loss = criterion(logits, yb)       # yb: [B]
            if train_mode:
                loss.backward()
                optimizer.step()

            total_loss += loss.item() * xb.size(0)
            preds = logits.argmax(dim=1)
            y_pred.append(preds.detach().cpu().numpy())
            y_true.append(yb.detach().cpu().numpy())
            N_correct += (preds == yb).sum().item()
            N_count += xb.size(0)

        return total_loss / N_count, N_correct / N_count, \
            np.concatenate(y_pred), np.concatenate(y_true)

    # dimension reduction
    X_train = X_train[:, clip_range]

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

    model = model.to(device)
    print(summary(model))

    rng = torch.Generator().manual_seed(seed)

    # Split
    sss = StratifiedShuffleSplit(n_splits=1, test_size=val_split, random_state=seed)
    train_idx, val_idx = next(sss.split(X_train, y_train))

    # Fit scaler on *train* only
    scaler = StandardScaler().fit(X_train[train_idx])
    train_ds = preprocessing(X_train[train_idx], y_train[train_idx], scaler)
    val_ds   = preprocessing(X_train[val_idx], y_train[val_idx], scaler)

    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True, generator=rng)
    val_loader   = DataLoader(val_ds,   batch_size=batch_size, shuffle=False)

    criterion = nn.CrossEntropyLoss()
    optimizer = torch.optim.Adam(model.parameters(),
                                 lr=learning_rate, weight_decay=weight_decay)

    early_stopping = EarlyStopping(patience=15)

    # Training / Validation
    for epoch in range(1, num_epochs + 1):
        train_loss, train_acc, _, _ = run_epoch(train_loader, train_mode=True)
        val_loss, val_acc, y_pred, y_true   = run_epoch(val_loader)
        print(f"Epoch {epoch:02d} | "
              f"train loss {train_loss:.4f} acc {train_acc:.4f} | "
              f"val loss {val_loss:.4f} acc {val_acc:.4f}")
        if early_stopping.step(val_loss, model):
            print("Early stopping triggered.")
            break

    # restore best weights
    early_stopping.restore(model)
    return model, scaler


def classifier_inference(X_test, clip_range, model, scaler,
                         batch_size=32):
    """
    Perform inference on the test dataset using the provided model.

    Parameters
    ----------
    X_test : np.ndarray
        The test input features.
    clip_range : slice
        The range of features to be used from X_test.
    model : torch.nn.Module
        The trained PyTorch model for classification.
    scaler : sklearn.preprocessing
        Scaler used to preprocess the data.
    batch_size : int, optional
        The number of samples per batch for loading data (default is 32).

    Returns
    -------
    y_prob : np.ndarray
        Shape (#sample, #class), the predicted probabilities for each class
        across all test samples.
    """
    # pre-process
    X_test = X_test[:, clip_range]
    X = scaler.transform(X_test)
    N, L = X.shape
    X = X.reshape(N, L, 1).transpose(0, 2, 1)         # [N, C=1, L]
    test_ds = TensorDataset(torch.tensor(X, dtype=torch.float32))
    test_loader = DataLoader(test_ds,  batch_size=batch_size, shuffle=False)

    # model
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = model.to(device)
    model.eval()

    # inference
    y_prob = []
    with torch.inference_mode():
        for xb, in test_loader:
            # xb: x-batch, shape [B, C, L]
            xb = xb.to(device)
            logits = model(xb)                 # [B, N_class]
            # collect the probability
            probs = torch.softmax(logits, dim=1)
            y_prob.append(probs.detach().cpu().numpy())
    return np.concatenate(y_prob, axis=0)


def classifier_report(X_test, y_test, clip_range, model, scaler,
                      batch_size=32, verbose=False):
    """
    Evaluate the classifier on the test dataset and print loss and accuracy.

    Parameters
    ----------
    X_test : np.ndarray
        The test input features.
    y_test: np.ndarray
        The true labels for the test dataset.
    clip_range : slice
        The range of features to be used from X_test.
    model : torch.nn.Module
        The trained PyTorch model for classification.
    scaler : sklearn.preprocessing
        Scaler used to preprocess the data.
    batch_size : int, optional
        The number of samples per batch for loading data (default is 32).

    Prints
    ------
    - Test set loss.
    - Test set accuracy.
    - Classification report: precision, recall, and F1-score for each class.
    """
    def run_epoch(loader):
        total_loss, N_correct, N_count = 0.0, 0, 0
        y_pred, y_true = [], []
        for xb, yb in loader:
            # xb: x-batch, shape [B, C, L]
            # yb: y-batch, shape [B]
            xb, yb = xb.to(device), yb.to(device)
            logits = model(xb)                 # [B, N_class]
            loss = criterion(logits, yb)       # yb: [B]

            # counts statistics
            total_loss += loss.item() * xb.size(0)
            preds = logits.argmax(dim=1)
            y_pred.append(preds.detach().cpu().numpy())
            y_true.append(yb.detach().cpu().numpy())
            N_correct += (preds == yb).sum().item()
            N_count += xb.size(0)

        return total_loss / N_count, N_correct / N_count, \
            np.concatenate(y_pred), \
            np.concatenate(y_true)

    # pre-process
    X_test = X_test[:, clip_range]
    X = scaler.transform(X_test)
    N, L = X.shape
    X = X.reshape(N, L, 1).transpose(0, 2, 1)         # [N, C=1, L]
    test_ds = TensorDataset(torch.tensor(X, dtype=torch.float32),
                            torch.tensor(y_test, dtype=torch.long))
    test_loader = DataLoader(test_ds,  batch_size=batch_size, shuffle=False)

    # model
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = model.to(device)
    model.eval()

    # define the loss function
    criterion = nn.CrossEntropyLoss()

    # Evaluate on test set
    with torch.no_grad():
        loss, acc, y_pred, y_true = run_epoch(test_loader)

    print(f"Test set LOSS: {loss:.4f}")
    print(f"Test set ACCURACY: {acc:.4f}")
    if verbose:
        print("Test set classification report")
        print(classification_report(y_true, y_pred, digits=6))
