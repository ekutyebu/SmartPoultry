/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./src/pages/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/components/**/*.{js,ts,jsx,tsx,mdx}",
    "./src/app/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        background: "#0b0f19",
        cardBg: "rgba(17, 24, 39, 0.7)",
        borderColor: "rgba(31, 41, 55, 0.6)",
        accentBlue: {
          500: "#3b82f6",
          600: "#2563eb",
          400: "#60a5fa",
        },
        poultryGreen: "#10b981",
        poultryRed: "#ef4444",
        poultryOrange: "#f59e0b",
      },
      backgroundImage: {
        "gradient-radial": "radial-gradient(var(--tw-gradient-stops))",
        "gradient-conic":
          "conic-gradient(from 180deg at 50% 50%, var(--tw-gradient-stops))",
      },
    },
  },
  plugins: [],
}
