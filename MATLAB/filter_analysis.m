%% filter_analysis.m
%
% Theoretical model of the 2nd-order Sallen-Key analog front end.
% No hardware or captured data required -- this computes the expected
% system response from the component values in the schematic and
% compares it against the LTspice AC-sweep export.

clear; clc; close all;

%% ---- Circuit parameters (from schematic / README) ----------------
R_top    = 16.455e3;   % divider top resistor (ohm)
R_bottom = 10e3;        % divider bottom resistor (ohm)

R_filt   = 5.36e3;       % Sallen-Key R1 = R2 (ohm)
C_filt   = 100e-9;        % Sallen-Key C1 = C2 (F)

Rf       = 5.9e3;          % Sallen-Key gain feedback resistor (ohm)
Rg       = 10e3;            % Sallen-Key gain ground resistor (ohm)

Vsupply  = 3.3;               % ADC / MCU supply (V)
Vin_max  = 5.0;                 % max input signal (V)

%% ---- Derived quantities -------------------------------------------
divider_ratio = R_bottom / (R_top + R_bottom);
K_filter      = 1 + (Rf / Rg);
fc            = 1 / (2 * pi * R_filt * C_filt);
system_gain   = divider_ratio * K_filter;

fprintf('--- Analog Front End: Theoretical Parameters ---\n');
fprintf('Divider ratio        : %.4f\n', divider_ratio);
fprintf('Filter gain (K)       : %.4f  (%.2f dB)\n', K_filter, 20*log10(K_filter));
fprintf('Filter cutoff (fc)     : %.1f Hz\n', fc);
fprintf('Total system gain      : %.4f  (%.2f dB)\n', system_gain, 20*log10(system_gain));
fprintf('Max ADC voltage @5V in : %.3f V\n\n', Vin_max * system_gain);

%% ---- Theoretical 2nd-order Sallen-Key magnitude response ----------
% Standard equal-component Sallen-Key transfer function:
%   H(s) = K / (1 + (3-K)*(s/wc) + (s/wc)^2)
% Magnitude at frequency f:
%   |H(f)| = K / sqrt( (1-(f/fc)^2)^2 + ((3-K)*(f/fc))^2 )

f = logspace(0, 5, 2000);      % 1 Hz to 100 kHz
x = f / fc;
Q_term = (3 - K_filter);

H_filter_mag = K_filter ./ sqrt((1 - x.^2).^2 + (Q_term .* x).^2);
H_filter_dB  = 20*log10(H_filter_mag);

H_total_mag = divider_ratio .* H_filter_mag;   % divider is freq-independent
H_total_dB  = 20*log10(H_total_mag);

%% ---- Plot: theoretical filter-only response ------------------------
% Note: yline/xline are not implemented in Octave, so reference lines
% are drawn with plot() instead -- this works in both MATLAB and Octave.
figure('Name', 'Theoretical Sallen-Key Response');
semilogx(f, H_filter_dB, 'b-', 'LineWidth', 1.5);
hold on;
passband_dB = 20*log10(K_filter);
semilogx([1 1e5], [passband_dB passband_dB], 'k--', 'DisplayName', 'Passband gain');
semilogx([fc fc], ylim, 'r--', 'DisplayName', sprintf('fc \\approx %.0f Hz', fc));
grid on;
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
title('Theoretical Sallen-Key Filter Response (Filter Stage Only)');
legend('|H(f)| filter only', 'Passband gain', 'fc', 'Location', 'southwest');
xlim([1 1e5]);

%% ---- Plot: theoretical TOTAL system response (divider + filter) ----
figure('Name', 'Theoretical Total System Response');
semilogx(f, H_total_dB, 'b-', 'LineWidth', 1.5);
hold on;
passband_total_dB = 20*log10(system_gain);
semilogx([1 1e5], [passband_total_dB passband_total_dB], 'k--', 'DisplayName', 'Passband gain');
semilogx([fc fc], ylim, 'r--', 'DisplayName', sprintf('fc \\approx %.0f Hz', fc));
grid on;
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
title('Theoretical Total Analog Front-End Response (Divider + Filter)');
legend('|H(f)| total system', 'Passband gain', 'fc', 'Location', 'southwest');
xlim([1 1e5]);


%% ---- Reference table: input vs expected ADC voltage -----------------
Vin_table = [0 1 2 2.5 3 4 5]';
Vadc_table = Vin_table .* system_gain;

fprintf('\n--- DC Reference Table ---\n');
fprintf('%8s %14s\n', 'Vin (V)', 'Expected Vadc (V)');
for k = 1:length(Vin_table)
    fprintf('%8.2f %14.3f\n', Vin_table(k), Vadc_table(k));
end
