import hashlib

from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
from Crypto.Signature import pkcs1_15

def generate_secret_key(seed_length=24):
    seed = derive_seed_from_file("post.bin")
    return hashlib.sha512(seed).digest()[:seed_length]

def derive_seed_from_file(filepath):
    with open(filepath, "rb") as f:
        data = f.read()
    return hashlib.sha256(data).digest()


def deterministic_rsa_keypair(seed):
    class DeterministicRNG:
        def __init__(self, seed):
            self.seed = seed
            self.state = hashlib.sha512(seed).digest()

        def get_bytes(self, n):
            result = b""
            while len(result) < n:
                self.state = hashlib.sha512(self.state).digest()
                result += self.state
            return result[:n]

    rng = DeterministicRNG(seed)
    return RSA.generate(2048, randfunc=rng.get_bytes)


def sign_message(message, private_key):
    h = SHA256.new(message)
    signature = pkcs1_15.new(private_key).sign(h)
    return signature


def verify_signature(message, signature, public_key):
    try:
        h = SHA256.new(message)
        pkcs1_15.new(public_key).verify(h, signature)
        return True
    except (ValueError, TypeError):
        return False
    except Exception as e:
        print(f"Błąd weryfikacji: {str(e)}")
        return False

def get_public_key_pem(private_key):
    return private_key.publickey().export_key()