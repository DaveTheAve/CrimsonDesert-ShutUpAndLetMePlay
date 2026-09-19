<h2>For Crimson Desert</h2>

<p>Have you ever installed a <strong>cutscene skip</strong> mod only to discover that its idea of skipping is making everyone in the cutscene speak and move like they are late for a dentist appointment?</p>

<p>That is <strong>fast-forward</strong>.</p>

<p>This mod does something dangerously literal:</p>

<h2>It actually skips the cutscene.</h2>

<p>Hold the native Skip button and the cutscene ends. Gameplay returns. Civilization advances slightly.</p>

<h3>What it does</h3>
<ul>
<li>Restores Crimson Desert's existing native Skip control and prompt during supported normal gameplay cutscenes.</li>
<li>Uses the game's own hold-to-skip action, progress, bindings, glyphs, localization, and UI scaling.</li>
<li>Keeps the native interface instead of drawing a fake Skip overlay on top.</li>
<li>Lets <strong>you</strong> decide when to skip. This is not automatic cutscene extermination.</li>
</ul>

<h3>What it does not do</h3>
<ul>
<li>Fast-forward the cutscene and call it skipping. We have standards. Very low standards, perhaps, but this is one of them.</li>
<li>Remap your controls.</li>
<li>Change the game's native hold duration.</li>
<li>Add telemetry, an updater, an account, a subscription, or Skip Button Plus.</li>
<li>Patch <code>CrimsonDesert.exe</code> on disk or edit your saves.</li>
</ul>

<h2>Requirements</h2>
<ul>
<li>Windows x64 Crimson Desert</li>
<li>A compatible x64 ASI loader (not included)</li>
</ul>

<h2>Installation - the serious part</h2>
<ol>
<li>Completely close Crimson Desert.</li>
<li>Keep your existing working ASI loader. New installations need a compatible x64 loader first. <a href="https://github.com/ThirteenAG/Ultimate-ASI-Loader">Ultimate ASI Loader</a> is one option. Do not overwrite another mod's existing proxy DLL/loader.</li>
<li>Copy <code>ShutUpAndLetMePlay.asi</code> beside <code>CrimsonDesert.exe</code>, normally in <code>bin64</code>.</li>
<li>Launch normally.</li>
<li>Find a supported gameplay cutscene, hold the native Skip button, and enjoy the shortest director's cut ever made.</li>
</ol>

<p>Only the ASI is installed. Do not replace the game executable. Keep one copy across all loader-scanned folders. Put backups outside those folders; adding <code>_backup</code> to an <code>.asi</code> filename does not make an ASI loader suddenly respect boundaries.</p>

<h2>Compatibility</h2>

<h3>Built to survive game updates</h3>

<p><strong>Shut Up and Let Me Play does not rely on fixed memory addresses or a fixed executable layout.</strong> At startup, it pattern-scans <code>CrimsonDesert.exe</code> for the native functions it needs, resolves their current locations, and verifies the surrounding code before installing anything.</p>

<p>That means many game updates that simply move code around, change RVAs, or rearrange the executable should require <strong>no mod update at all</strong>. The mod finds the functions again wherever the update moved them and carries on.</p>

<p>If Pearl Abyss substantially rewrites the relevant native code, a signature becomes ambiguous, or another mod has already modified the same entry points, the mod fails safely instead of hooking an unknown function and hoping for the best. At that point, an update may be required. Crashing faster is also not skipping.</p>

<p>The currently tested <strong><code>CrimsonDesert.exe</code> file version is <code>1.0.0.2944</code></strong>. That version number records what has been tested; it is <strong>not</strong> a hard-coded compatibility lock.</p>

<p>The supported normal gameplay cinematic path is the target. Not every scene, device, remap, language, UI scale, store build, or future patch has been personally interrogated.</p>

<h2>Uninstallation</h2>
<p>Close the game and remove <code>ShutUpAndLetMePlay.asi</code>. You may also remove:</p>
<ul>
<li><code>ShutUpAndLetMePlay.log</code></li>
<li><code>ShutUpAndLetMePlay_UpdateReport.json</code></li>
</ul>
<p>Leave your ASI loader installed if other mods still need it.</p>

<h2>FAQ</h2>

<h3>Does it really skip instead of fast-forwarding?</h3>
<p>Yes. The title is obnoxiously specific for a reason.</p>

<h3>Does it auto-skip everything?</h3>
<p>No. Some cutscenes are good. You remain responsible for your own attention span.</p>

<h3>Why do I have to hold the button?</h3>
<p>Because that is the game's native Skip behavior. This mod restores it rather than replacing it with a custom instant-skip system.</p>

<h3>Will it work forever after every game update?</h3>
<p>No mod that hooks native game code can honestly promise that. This one rediscovers the functions it needs at runtime instead of relying on fixed addresses, so ordinary code relocation should not bother it. If the relevant functions materially change, it fails closed until the signatures can be updated.</p>

<h3>Why not use a fast-forward mod?</h3>
<p>You can! Fast-forward is useful. This is for people who looked at the word <strong>Skip</strong> and made the controversial decision to take it literally.</p>

<h2>Support</h2>
<p>For problems, include the scene, input device, game/mod/loader versions, relevant other mods, and <strong>both current files from beside the ASI</strong>:</p>
<ul>
<li><code>ShutUpAndLetMePlay.log</code></li>
<li><code>ShutUpAndLetMePlay_UpdateReport.json</code></li>
</ul>

<p><code>active</code> means the native hooks installed; it is not proof that diagnostic software personally observed the pixels on your monitor. <code>disabled</code> includes a reason. If you see <code>active_warning</code>, exit the game and remove the ASI before relaunching.</p>

<p>Do not upload <code>CrimsonDesert.exe</code>, saves, or account data. An unwritable game folder can prevent diagnostics from being written.</p>

<h2>Source</h2>
<p>GitHub repository: <a href="https://github.com/DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay">DaveTheAve/CrimsonDesert-ShutUpAndLetMePlay</a></p>
<p>Licensed under the <strong>MIT License</strong>.</p>

<hr>

<p><strong>Fast-forward</strong> makes the cutscene shorter.<br>
<strong>Shut Up and Let Me Play</strong> makes the cutscene somebody else's problem.</p>

<p><strong>Hold Skip. Resume game. Enjoy your time. #VivaLaSkip</strong></p>
