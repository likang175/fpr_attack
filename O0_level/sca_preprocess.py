"""Trace pre-processing."""

import time
import numpy as np
import scipy as sp
import itertools
import collections
import matplotlib.pyplot as plt
from sklearn.preprocessing import StandardScaler
from sklearn.decomposition import PCA


def trace_categorize(traces, label):
    """
    Categorize traces into groups based on their corresponding labels.

    Parameters
    ----------
    traces : np.ndarray
        Shape (#label x #trace, #sample).
    label : np.ndarray
        Shape (#label x #trace,).

    Returns
    -------
    traces_dict : dict[int: np.ndarray]
        Maps label (int) → traces (#trace, #sample):
        {
            label_1: np.ndarray,  # shape (#trace/label_1, #sample)
            label_2: np.ndarray,  # shape (#trace/label_2, #sample)
            ...,
            label_n: np.ndarray   # shape (#trace/label_n, #sample)
        }
    """
    traces_dict = collections.defaultdict(list)
    for trace, lbl in zip(traces, label):
        traces_dict[lbl].append(trace)
    traces_dict = {k: np.stack(v) for k, v in traces_dict.items()}
    return traces_dict


def trace_fft(trace, sample_freq):
    """Perform FFT to trace."""
    N = len(trace)
    # Apply FFT
    fft_vals = np.fft.fft(trace)
    fft_freqs = np.fft.fftfreq(N, d=1/sample_freq)
    # Take the first half, due to symmetry
    half_N = N // 2
    fft_magnitude = np.abs(fft_vals[:half_N])
    fft_freqs = fft_freqs[:half_N]
    return fft_freqs, fft_magnitude


def trace_butter_lpf(trace, sample_freq, cutoff_freq, order=4):
    """
    Apply low passed filter to trace to rm high freq noise.

    Parameters
    ----------
    trace : np.ndarray
        Shape (#label x #trace, #sample), signal to be filtered.
    sample_freq : float
        The sampling rate of the input signal in Hz, E.g. 30MHz.
    cutoff_freq : float
        The cutoff frequency for the low pass filter in Hz.
    order : int, optional
        The order of the Butterworth filter (default is 4).

    Returns
    -------
    trace_lpf : np.ndarray
        Shape (#label x #trace, #sample), filtered signal.
    """
    nyquist = 0.5 * sample_freq
    normal_cutoff = cutoff_freq / nyquist
    b, a = sp.signal.butter(order, normal_cutoff, btype='low', analog=False)
    # apply to every trace along the sample axis
    trace_lpf = sp.signal.filtfilt(b, a, trace, axis=1)
    return trace_lpf


###############################################################################
#                               t-test, SOD, SNR                              #
###############################################################################

def calc_ttest(traces_dict):
    """
    Calculate sample-wise t-test scores between all class pairs.

    This function computes the Welch t-statistics for all unique pairs of
    groups from the input data (traces) at each sample point. It uses 2-sample
    t-test, assuming unequal variances (Welch t-test). The resulting t-scores
    are accumulated for all combinations of groups.

    Parameters
    ----------
    traces_dict : dict[int: np.ndarray]
        Maps label (int) → traces (#trace, #sample):
        {
            label_1: np.ndarray,  # shape (#trace/label_1, #sample)
            label_2: np.ndarray,  # shape (#trace/label_2, #sample)
            ...,
            label_n: np.ndarray   # shape (#trace/label_n, #sample)
        }

    Returns
    -------
    t_score : np.ndarray
        Shape (#sample,), t-statistic values.
    """
    label = sorted(traces_dict.keys())
    N_sample = traces_dict[label[0]].shape[1]

    t_score = np.zeros(N_sample)
    for l1, l2 in itertools.combinations(label, 2):  # iterate  all label pairs
        t_vals, _ = sp.stats.ttest_ind(traces_dict[l1],
                                       traces_dict[l2], equal_var=False)
        t_vals = np.abs(t_vals)
        if np.isnan(t_vals).any():
            print(f"NaNs detected in t-test between label {l1} and {l2}")
        # t_vals = np.nan_to_num(np.abs(t_vals))  # replae NaN to 0
        t_score += t_vals  # accumulate t-val for each combination
    print(f"Max t-score: {np.max(t_score):.4f}")
    return t_score


def calc_sod(traces_dict):
    """
    Calculate Sum of Differences (SOD) across class means.

    Parameters
    ----------
    traces_dict : dict[int: np.ndarray]
        Maps label (int) → traces (#trace, #sample):
        {
            label_1: np.ndarray,  # shape (#trace/label_1, #sample)
            label_2: np.ndarray,  # shape (#trace/label_2, #sample)
            ...,
            label_n: np.ndarray   # shape (#trace/label_n, #sample)
        }

    Returns
    -------
    sod : np.ndarray
        Shape (#sample,), sum of absolute differences of class means.
    """
    label = sorted(traces_dict.keys())
    N_sample = traces_dict[label[0]].shape[1]

    mean = {}
    for i in label:
        mean[i] = np.mean(traces_dict[i], axis=0)

    sod = np.zeros(N_sample)
    for l1, l2 in itertools.combinations(label, 2):  # iterate  all label pairs
        sod += np.abs(mean[l1] - mean[l2])
    print(f"Max SOD: {np.max(sod):.4f}")
    return sod


def calc_snr(traces_dict):
    """
    Calculate SNR using grouped trace dictionary.

    Parameters
    ----------
    traces_dict : dict[int: np.ndarray]
        Maps label (int) → traces (#trace, #sample):
        {
            label_1: np.ndarray,  # shape (#trace/label_1, #sample)
            label_2: np.ndarray,  # shape (#trace/label_2, #sample)
            ...,
            label_n: np.ndarray   # shape (#trace/label_n, #sample)
        }

    Returns
    -------
    snr : np.ndarray
        Shape (#sample,), the SNR at each sample point.
    """
    # Compute class mean & var across traces
    class_means = []
    class_vars = []
    for label, traces in traces_dict.items():
        class_means.append(np.mean(traces, axis=0))
        class_vars.append(np.var(traces, axis=0))
    class_means = np.vstack(class_means)  # (#class, #sample)
    class_vars = np.vstack(class_vars)    # (#class, #sample)

    # Compute the mean & var across label
    signal_var = np.var(class_means, axis=0)  # (#sample,)
    noise_var = np.mean(class_vars, axis=0)   # (#sample,)

    snr = signal_var / noise_var
    print(f"Max SNR: {np.max(snr):.4f}")
    return snr


###############################################################################
#                                 select POIs                                 #
###############################################################################

def select_poi_max_rank(array, N_poi, poi_space):
    """
    Select POI from the array acc.to. max rank.

    Parameters
    ----------
    array : ndarray
        Shape (#sample,), the t-score or SNR or SOD.
    N_poi : int
        The maximum number of POIs to select from the array.
    poi_space : int
        The number of adjacent indices (before and after) that should not be
        selected once a POI has been specified.
        This parameter addresses the tendency for maximum values in the array
        to cluster together.
        By setting 'poi_space' to a positive integer (e.g., 3), one can
        mitigate the over-concentration of selected points and ensure diversity
        in the selection. The following illustration depicts this concept:

               |
              ||
              |||
              ||||      |
              ||||      ||
        |     ||||     |||
        ||    ||||     ||||

        It's recommended to adjust 'poi_space' based on empirical observations.

    Returns
    -------
    poi : list[int]
        A list of indices representing the selected POIs. All entries are
        integers within the range of #sample.
    """
    array_tmp = array.copy()

    poi = []

    for i in range(N_poi):
        nextPOI = np.argmax(array_tmp)
        poi.append(int(nextPOI))
        poiMin = max(0, nextPOI - poi_space)
        poiMax = min(nextPOI + poi_space, len(array_tmp) - 1)
        for j in range(poiMin, poiMax+1):
            array_tmp[j] = 0
    return poi


def select_poi_threshold(array, threshold):
    """
    Select POI from the array if larger than threshold.

    Parameters
    ----------
    array : np.ndarray
        Shape (#sample,), the t-score or SNR or SOD.
    threshold: np.ndarray or int
        Could be const array [0.001]*#sample or t-score.

    Returns
    -------
    poi : list[int]
        A list of indices representing the selected POIs. All entries are
        integers within the range of #sample.
    """
    return np.where(array > threshold)[0]


###############################################################################
#                               Template Attack                               #
###############################################################################


def template_build(traces_dict, poi):
    """
    Build mean and covariance templates per class at selected POIs.

    Parameters
    ----------
    traces_dict : dict[int: np.ndarray]
        Maps label (int) → traces (#trace, #sample):
        {
            label_1: np.ndarray,  # shape (#trace/label_1, #sample)
            label_2: np.ndarray,  # shape (#trace/label_2, #sample)
            ...,
            label_n: np.ndarray   # shape (#trace/label_n, #sample)
        }
    poi : list[int]
        List of sample indices to extract as POIs

    Returns
    -------
    template : tuple
        A tuple of (f, poi), where:
        - f : dict[int: sp.stats.multivariate_normal]
            Maps label (int) → f(x|theta), where theta = (mean, cov)
        - poi : list[int]
            The POI indices.
    Notes
    -----
    'f' is mathematically f(x|theta), where theta = (mean, cov), constructed by
        f[c] = sp.stats.multivariate_normal(mean[c], cov[c])
    where
    mean : dict[int: np.ndarray]
        Maps label (int) → poi mean (#poi,)
        {
            label_1: np.ndarray,  # shape (#poi,)
            label_2: np.ndarray,  # shape (#poi,)
            ...,
            label_n: np.ndarray,  # shape (#poi,)
        }
    cov: dict[int: np.ndarray]
        Maps label (int) → poi cov (#poi, #poi)
        {
            label_1: np.ndarray,  # shape (#poi, #poi)
            label_2: np.ndarray,  # shape (#poi, #poi)
            ...,
            label_n: np.ndarray,  # shape (#poi, #poi)
        }

    Therefore we can extract mean and cov via
    - mean = {k: v.mean for k, v in f.items()}
    - cov = {k: v.cov for k, v in f.items()}
    """
    label = sorted(traces_dict.keys())
    N_poi = len(poi)

    trace_mean = {}
    mean = {}
    cov = {}
    f = {}
    for c in label:
        trace_mean[c] = np.mean(traces_dict[c], axis=0)
        mean[c] = np.zeros((N_poi))
        cov[c] = np.zeros((N_poi, N_poi))
        for i in range(N_poi):
            mean[c][i] = trace_mean[c][poi[i]]
            for j in range(N_poi):
                xi = traces_dict[c][:, poi[i]]
                xj = traces_dict[c][:, poi[j]]
                cov[c][i, j] = np.cov(xi, xj)[0, 1]
        # f(x|theta[c]), theta[c] = (mean, cov)
        f[c] = sp.stats.multivariate_normal(mean[c], cov[c])
    return (f, poi)


# def template_attack(template, traces_test, label_test):
#     """
#     Perform template attack using multivariate Gaussian classifiers.

#     Parameters
#     ----------
#     template : tuple
#         A tuple of (f, poi), where:
#         - f : dict[int: sp.stats.multivariate_normal]
#             Maps label (int) → f(x|theta), where theta = (mean, cov)
#         - poi : list[int]
#             The POI indices.
#     traces_test : np.ndarray
#         Shape (#label x #trace, #sample), the traces to attack
#     label_test : np.ndarray
#         Shape (#label x #trace,), the label of the attacked traces

#     Returns
#     -------
#     guess : np.ndarray
#         Shape (#label x #trace,), the guess label
#     """
#     label = sorted(set(label_test))
#     N_label = len(label)
#     f, poi = template
#     total = len(label_test)  # #label x #trace
#     epsilon = 1e-300

#     guess = []
#     for i in range(total):
#         # Compute log-likelihood for each class
#         log_likelihoods = []
#         for j in range(N_label):
#             c = label[j]
#             x = traces_test[i, poi]
#             prob = f[c].pdf(x)  # f(x|theta)
#             log_likelihoods.append(np.log(prob + epsilon))
#         guess.append(label[np.argmax(log_likelihoods)])
#     return np.array(guess)

def template_attack(template, traces_test, all_labels):
    """
    Perform template attack using multivariate Gaussian classifiers.

    Parameters
    ----------
    template : tuple
        A tuple of (f, poi), where:
        - f : dict[int: sp.stats.multivariate_normal]
            Maps label (int) → f(x|theta), where theta = (mean, cov)
        - poi : list[int]
            The POI indices.
    traces_test : np.ndarray
        Shape (#label x #trace, #sample), the traces to attack
    label_test : np.ndarray
        Shape (#label x #trace,), the label of the attacked traces

    Returns
    -------
    guess : np.ndarray
        Shape (#label x #trace,), the guess label
    """
    label = sorted(set(all_labels))
    N_label = len(label)
    f, poi = template
    # total = len(label_test)  # #label x #trace
    total = traces_test.shape[0]
    epsilon = 1e-300

    guess = []
    for i in range(total):
        # Compute log-likelihood for each class
        log_likelihoods = []
        for j in range(N_label):
            c = label[j]
            x = traces_test[i, poi]
            prob = f[c].pdf(x)  # f(x|theta)
            log_likelihoods.append(np.log(prob + epsilon))
        guess.append(label[np.argmax(log_likelihoods)])
    return np.array(guess)


def template_attack_report(guess, label_test):
    """
    Check the success rate and error guess of the template attack.

    Parameters
    ----------
    guess : np.ndarray
        Shape (#label x #trace,), the guess label
    label_test : np.ndarray
        Shape (#label x #trace,), the label of the attacked traces

    Returns
    -------
    None
    """
    total = len(label_test)  # #label x #trace
    success = 0
    error_guess = []

    for i in range(total):
        if guess[i] == label_test[i]:
            success += 1
        else:
            error_guess.append(int(label_test[i]))

    print(f"#Success: {success}")
    print(f"%Success: {success / total:.2%}")
    print(f"Error guess: {error_guess}")
    print(f"Counter: {collections.Counter(error_guess)}")


###############################################################################
#                                   old code                                  #
###############################################################################

def preprocess(feature_set, label_set,
               sample_rate, cutoff_freq, order,
               N_poi, poi_spacing, if_debug=True):
    """
    Pre-processing.

    Parameters
    ----------
    feature_set : ndarray of shape (#trace, #sample)
    label_set : ndarray of shape (#trace,)

    Returns
    -------
    feature_lpf_poi : ndarray of shape (#trace, #sample)

    Notes
    -----
    We do not process the 'label_set' here.
    """
    trace_fft(feature_set[0], sample_rate, "FFT")
    feature_lpf = np.array([trace_butter_lpf(aa, sample_rate, cutoff_freq, order)
                            for aa in feature_set])
    if if_debug:
        plt.figure(figsize=(14.4, 4.8))
        plt.plot(feature_set[0], lw=0.5, label='origin', alpha=0.5)
        plt.plot(feature_lpf[0], lw=1, label='low pass filtered')
        plt.xlabel("Samples")
        plt.ylabel("Power trace data")
        plt.show()
        plt.close()
    # Get POI by t-test #######################################################
    # Group traces to the same class
    """
    Create a table of the following form
    {0: trace_array0 (shape (6244, #sample)),
     1: trace_array1 (shape (5344, #sample)),
     ...,
     7: trace_array7 (shape (   3, #sample)),
     8: trace_array8 (shape (   1, #sample))}
    i.e. to group traces to the same class (in order to do t-test), and
      6244 + 5344 + ... + 3 + 1 = #trace
    """
    feature_lpf_table = {
        cls: feature_lpf[label_set == cls]
        for cls in np.unique(label_set)
    }
    if if_debug:
        for cls in range(len(set(label_set))):
            print(f"{cls}: {feature_lpf_table[cls].shape}")
    # t-test
    t_score = calc_ttest(feature_lpf_table)
    poi = select_poi_max_rank(t_score, N_poi, poi_spacing)
    feature_lpf_poi = feature_lpf[:, poi]
    return feature_lpf_poi


def preprocessing(train_traces, test_traces, poi, n_components=0.99):
    """Pre-processing: SNR to pick POI → scale → PCA."""
    scaler = StandardScaler()
    train_traces_std = scaler.fit_transform(train_traces[:, poi])
    test_traces_std = scaler.transform(test_traces[:, poi])
    print(time.asctime())
    pca = PCA(n_components)
    train_traces_pca = pca.fit_transform(train_traces_std)
    test_traces_pca = pca.transform(test_traces_std)
    print(time.asctime())
    print(f"Selected #component {pca.n_components_}")
    return train_traces_pca, test_traces_pca
