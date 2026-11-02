# P01 Literature Notes: Hammer–String and Stiff-String Reference

## Source findings

- Bank’s thesis models felt compression with the power law $F=K(\Delta y)^p$ and says $K,p$ should be fitted from measurements. It describes the hammer as a lumped mass, the felt as a nonlinear spring, and notes that real felt is hysteretic. Its ideal-string derivation assumes small transverse slope, uniform tension and linear density, and one polarization. See [Bank 2000, §2.1.2, Eq. (2.1), pp. 19–20, and §4.1, Eq. (4.1), pp. 36–37](https://home.mit.bme.hu/~bank/thesis/pianomod.pdf).
- The 2003 review defines $\Delta y=y_h-y_s$, applies the contact law only for $\Delta y>0$, and sets force to zero after separation. Its example C4 values are $p=2.5$, $k=4.5\times10^9$, and $m_h=2.97\times10^{-3}\,\mathrm{kg}$; these are literature examples, not Aether Piano measurements. Table 1 gives units for hammer mass but not $k$; dimensional analysis requires $\mathrm{N\,m^{-p}}$ when displacement is in metres. The review also explains that a one-sample delayed force can destabilize the coupled model; implicit contact treatment or a faster hammer update are documented alternatives. See [Bank et al. 2003, Table 1 and §§3.1–3.2, Eqs. (1)–(5), pp. 943–944](https://home.mit.bme.hu/~bank/publist/jasp03.pdf).
- Smith’s stiff-string model adds a fourth spatial derivative to the ideal-string wave equation. Its partial-frequency approximation is $f_n=n f_0\sqrt{1+Bn^2}$, with dimensionless $B$; the piano-string section connects $B$ to stiffness, tension, and length. The same source says stiffness makes higher-frequency components travel faster. See [PASP author version, “The Stiff String”](https://ccrma.stanford.edu/~jos/pasp/Stiff_String.html), [“Stiff Piano Strings,” Eqs. (10.30)–(10.33)](https://ccrma.stanford.edu/~jos/pasp/Stiff_Piano_Strings.html) ([mirror](https://www.dsprelated.com/freebooks/pasp/Stiff_Piano_Strings.html)). Smith’s hammer section gives $F_h=Q_0x_k^p$, $p>2$ for typical pianos, and discusses a hysteresis correction; it lists $Q_0$ in $\mathrm{N/mm^p}$, so its compression coordinate is in millimetres unless converted. See [“Piano Hammer Modeling,” Eqs. (10.20)–(10.21)](https://ccrma.stanford.edu/~jos/pasp/Piano_Hammer_Modeling.html).

## P01 reference convention and units

Use one declared transverse axis, positive in the hammer’s approach direction. Let $q_h(t)$ be hammer-face displacement and $y(x,t)$ string displacement along that axis, with zero compression at first touch and strike location $x_i$. Then

\[
\delta=\max(0,q_h-y(x_i,t)),\qquad F_c=K_h\delta^p,
\]

where $q_h,y,\delta$ are metres, $F_c$ is newtons, $p$ is dimensionless, $K_h$ has units $\mathrm{N\,m^{-p}}$, and $m_h$ is in kilograms. Hammer/string velocities are in $\mathrm{m/s}$. The contact force on the hammer is $-F_c$, so $m_h\ddot q_h=-F_c$; the string receives $+F_c$. This matches Bank’s compression sign and action-reaction convention. The source papers reuse $K$ for hammer stiffness and string tension; use $K_h$ and $T$ separately.

For a one-polarization string reference, use

\[
\mu y_{tt}=T y_{xx}-D y_{xxxx}+F_c\,\delta_D(x-x_i),\quad D=E I_A,
\]

Here $\mu$ is linear density ($\mathrm{kg/m}$), $T$ is tension ($\mathrm N$), $E$ is Young’s modulus ($\mathrm{N/m^2}$), and $I_A$ is the cross-section area second moment ($\mathrm{m^4}$); therefore bending rigidity $D=EI_A$ is in $\mathrm{N\,m^2}$. The Dirac delta has units $\mathrm{m^{-1}}$, so the point-force source and every PDE term have units $\mathrm{N/m}$. The ideal-string baseline sets $D=0$. For the finite reference, pinned ends $y=0,\ y_{xx}=0$ at $x=0,L$ are an explicit P01 boundary assumption; they are not a measured piano bridge/agraffe model.

For these pinned modes, $f_0=(2L)^{-1}\sqrt{T/\mu}$ and $B=\pi^2D/(TL^2)$, giving $f_n=nf_0\sqrt{1+Bn^2}$. These are mode predictions for this boundary assumption, not fitted C4 data.

With no damping or external action during contact, the audit energy is

\[
E=\tfrac12m_h\dot q_h^2+\int_0^L\!\left(\tfrac12\mu y_t^2+\tfrac12T y_x^2+\tfrac12D y_{xx}^2\right)dx+\frac{K_h\delta^{p+1}}{p+1}.
\]

The final term is the conservative felt potential implied by the memoryless power law. Correct equal-and-opposite coupling conserves this total; losses or hysteresis require a separate dissipative-energy account.

## Transferable results and limits

Transfer the unilateral power-law contact, measured hammer velocity as the physical excitation control, energy bookkeeping, and the stiff-string partial law. Bank’s model uses hammer impact velocity as its physical control (§5.1–5.2, pp. 48–50), and reports a typical strike about one-seventh along the string (§2.6, p. 30); use that only as a provisional geometry assumption. Choose synthetic hammer speeds explicitly; the papers do not supply an Aether Piano MIDI-velocity mapping. [Bank 2000](https://home.mit.bme.hu/~bank/thesis/pianomod.pdf)

The point-mass/power-law felt omits felt hysteresis and internal modes, tension modulation, longitudinal motion, the second transverse polarization, and bridge/soundboard feedback. Bank’s thesis notes that its practical model combines physical and nonphysical blocks (§7.1); it is a modeling precedent, not evidence that the reduced system is a complete piano. Do not copy tabulated $K$ values without matching displacement units and instrument geometry. Smith’s rendered Eq. (10.33) also has an internal algebraic mismatch: with its stated circular-wire definitions $S=\pi a^2$ and radius of gyration $a/2$, the first expression implies denominator 4, while the displayed radius form shows 16. Use the dimensionally consistent $D=EI_A$ form and verify any radius-based coefficient against a typeset source before parameterizing it.
