# GitHub publishing guide

## Repository settings

- Name: `dice-vessel`
- Visibility: Public
- Description: `A pocket RPG campaign companion for fast, flexible dice rolls on the M5Stack Cardputer.`
- Default branch: `main`
- License: MIT — already included in `LICENSE`
- Topics: `m5stack`, `cardputer`, `esp32-s3`, `rpg`, `dice-roller`, `platformio`, `arduino`, `embedded`, `pixel-art`

## 1. Create the empty repository

1. Sign in to GitHub.
2. Select **New repository**.
3. Choose your account as owner.
4. Enter `dice-vessel` as the repository name.
5. Paste the description above.
6. Select **Public**.
7. Do **not** add a README, `.gitignore`, or license on GitHub; these already exist locally.
8. Select **Create repository**.

## 2. Send the prepared project

Open PowerShell inside the local `DiceVessel` folder and run:

```powershell
git init -b main
git add .
git commit -m "Initial DICE\\VESSEL beta release"
git remote add origin https://github.com/YOUR-USERNAME/dice-vessel.git
git push -u origin main
```

Replace `YOUR-USERNAME` with your GitHub username. GitHub may open a browser window for authentication.

## 3. Configure the repository page

1. Open the repository on GitHub.
2. In the **About** panel, select the edit icon.
3. Confirm the description.
4. Add the suggested topics.
5. Leave the website field empty for now.
6. Confirm that the README displays the screenshots and language link correctly.
7. Confirm that GitHub identifies the MIT license.

## 4. Publish the first beta release

1. Open **Releases** on the repository page.
2. Select **Draft a new release**.
3. Create the tag `v0.4.0-beta` targeting `main`.
4. Use the title `DICE\\VESSEL v0.4.0-beta — KEEP ROLLING.`
5. Copy the contents of `RELEASE_NOTES.md` into the description.
6. Attach these three files:
   - `release/dicevessel-v0.4-beta-factory.bin`
   - `release/dicevessel-v0.4-beta-firmware.bin`
   - `release/SHA256SUMS.txt`
7. Select **Set as a pre-release**.
8. Review the filenames and description.
9. Select **Publish release**.

## Final verification

- README in English opens by default.
- `README.pt-BR.md` opens from the Portuguese link.
- Screenshots render correctly.
- MIT license is detected.
- Issue templates appear under **New issue**.
- Factory image downloads from the release.
- Release is marked as pre-release.
- Tag is `v0.4.0-beta`.
