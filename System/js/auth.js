async function login() {
  const username = document.getElementById('username').value;
  const password = document.getElementById('password').value;

  try {
    const response = await fetch('Login_and_Password/Users.json');
    const users = await response.json();
    const user = users.find(u => u.username === username && u.password === password);

    if (user) {
      localStorage.setItem('authorized', 'true');
      window.location.href = 'DataControllers.html';
    } else {
      document.getElementById('error').innerText = 'Невірний логін або пароль';
    }
  } catch (err) {
    document.getElementById('error').innerText = 'Помилка при авторизації';
  }
}

function showHint() {
  document.getElementById('modal').style.display = 'block';
}

function closeHint() {
  document.getElementById('modal').style.display = 'none';
}

window.onclick = function(event) {
  const modal = document.getElementById('modal');
  if (event.target === modal) {
    modal.style.display = 'none';
  }
}
