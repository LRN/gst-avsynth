// Borrowed from the author of IT.dll, whose name I
// could not determine.

void DrawDigitYUY2(PVideoFrame &dst, int x, int y, int num) 
{
	extern unsigned short font[192][20];

	x = x * 10;
	y = y * 20;

	int pitch = dst->GetPitch();
	for (int tx = 0; tx < 10; tx++) {
		for (int ty = 0; ty < 20; ty++) {
			unsigned char *dp = &dst->GetWritePtr()[(x + tx) * 2 + (y + ty) * pitch];
			if (font[num][ty] & (1 << (15 - tx))) {
				if (tx & 1) {
					dp[0] = 250;
					dp[-1] = 128;
					dp[1] = 128;
				} else {
					dp[0] = 250;
					dp[1] = 128;
					dp[3] = 128;
				}
			} else {
				if (tx & 1) {
					dp[0] = (unsigned char) ((dp[0] * 3) >> 2);
					dp[-1] = (unsigned char) ((dp[-1] + 128) >> 1);
					dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
				} else {
					dp[0] = (unsigned char) ((dp[0] * 3) >> 2);
					dp[1] = (unsigned char) ((dp[1] + 128) >> 1);
					dp[3] = (unsigned char) ((dp[3] + 128) >> 1);
				}
			}
		}
	}
}


void DrawStringYUY2(PVideoFrame &dst, int x, int y, const char *s) 
{
	for (int xx = 0; *s; ++s, ++xx) {
		DrawDigitYUY2(dst, x + xx, y, *s - ' ');
	}
}

