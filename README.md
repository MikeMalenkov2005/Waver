# Waver

Waver is a Windows application for mixing waveforms.
It was created to develop the sound engine for my C games.

## Mixing sounds

Waver uses 16-bit PCM samples ands summes them to combine multiple sounds.
Other sound formats get converted to 16-bit PCM stereo when sampled.

## Multi-Threading

Waver utilizes one main thread for generic application logic and one additional thread for each active sound player.
The sound player threads are responsible for sending sound chunks and closing the sound devices.
