// Small analog clock for the "Logged at" stat card — hands actually point at the given
// hour/minute, same idea as WeatherIcon: an accurate glyph instead of a fixed emoji. (Unicode has
// 12 "clock face" emoji, one per hour with no minutes — not granular enough, and this matches the
// hand-drawn style already used for weather.) Original geometry, no icon library/asset.
export function ClockIcon({ hour, minute, size = 16 }: { hour: number; minute: number; size?: number }) {
  const minuteDeg = (minute / 60) * 360;
  const hourDeg = ((hour % 12) / 12) * 360 + (minute / 60) * 30;
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="none">
      <circle cx="12" cy="12" r="9.5" stroke="#9aa3c0" strokeWidth="1.6" />
      {/* hour hand: short, reaches to y=7 (radius 5) */}
      <line x1="12" y1="12" x2="12" y2="7" stroke="#e8eaf0" strokeWidth="1.8" strokeLinecap="round"
            transform={`rotate(${hourDeg} 12 12)`} />
      {/* minute hand: long, reaches to y=5 (radius 7) */}
      <line x1="12" y1="12" x2="12" y2="5" stroke="#e8eaf0" strokeWidth="1.4" strokeLinecap="round"
            transform={`rotate(${minuteDeg} 12 12)`} />
      <circle cx="12" cy="12" r="1.1" fill="#e8eaf0" />
    </svg>
  );
}
