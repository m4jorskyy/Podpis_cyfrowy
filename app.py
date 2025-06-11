import os
from flask import Flask, render_template, request, flash, session
from trng.run_trng import run_trng
from trng import rsa_utils
from Crypto.PublicKey import RSA

app = Flask(__name__)

if os.path.exists("post.bin"):
    app.secret_key = rsa_utils.generate_secret_key()
else:
    app.secret_key = os.urandom(24)

@app.route("/", methods=["GET", "POST"])
def index():
    return render_template("index.html")

@app.route("/sign", methods=["POST"])
def sign():
    # Generuj nowy post.bin
    run_trng()
    flash("Nowy post.bin wygenerowany", "info")

    # Pobierz przesłany plik
    file = request.files.get('file')
    if not file:
        flash("Nie przesłano pliku", "error")
        return render_template("index.html")

    filename = file.filename
    file_content = file.read()

    # Wygeneruj klucz na podstawie post.bin
    seed = rsa_utils.derive_seed_from_file("post.bin")
    key = rsa_utils.deterministic_rsa_keypair(seed)

    # Podpisz zawartość pliku
    signature = rsa_utils.sign_message(file_content, key)
    signature_hex = signature.hex()
    public_key_pem = key.publickey().export_key().decode('utf-8')

    # Zapisz dane w sesji
    session['signature'] = signature_hex
    session['public_key'] = public_key_pem
    session['filename'] = filename

    flash("Plik " + filename + " podpisany pomyślnie", "success")
    return render_template(
        "index.html",
        signed_filename=filename,
        signature=signature_hex,
        public_key=public_key_pem,
        show_verification=True
    )

@app.route("/verify", methods=["POST"])
def verify():
    file = request.files.get('file')
    signature_hex = request.form.get('signature_hex')
    public_key_pem = request.form.get('public_key_pem')

    # Sprawdzenie, czy wszystkie dane zostały dostarczone
    if not file or not signature_hex or not public_key_pem:
        flash("Brak wszystkich wymaganych danych do weryfikacji", "error")
        return render_template("index.html")

    # Odczyt zawartości pliku
    file_content = file.read()

    # Przetwarzanie podpisu i klucza publicznego
    try:
        signature = bytes.fromhex(signature_hex)
        public_key = RSA.import_key(public_key_pem)
    except ValueError as e:
        flash(f"Błąd przy przetwarzaniu podpisu lub klucza: {str(e)}", "error")
        return render_template("index.html")

    # Weryfikacja podpisu
    is_valid = rsa_utils.verify_signature(file_content, signature, public_key)
    result = "POPRAWNY" if is_valid else "NIEPOPRAWNY"
    color = "green" if is_valid else "red"

    # Wyświetlenie wyniku
    return render_template(
        "index.html",
        verification_result=result,
        verification_color=color
    )

if __name__ == "__main__":
    app.run(debug=True)