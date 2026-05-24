// JavaScript para pruebas MIME
console.log("JavaScript cargado");

function testJavaScript() {
    var elemento = document.getElementById('js-test');
    if (elemento) {
        elemento.innerHTML = 'JavaScript funciona correctamente (application/javascript)';
        elemento.style.color = '#28a745';
        elemento.style.fontWeight = 'bold';
    }
}

function loadTextFile() {
    fetch('/ejemplo.txt')
        .then(function(response) {
            if (!response.ok) throw new Error('Error ' + response.status);
            return response.text();
        })
        .then(function(text) {
            var output = document.getElementById('text-output');
            if (output) output.textContent = text;
        })
        .catch(function(error) {
            var output = document.getElementById('text-output');
            if (output) output.textContent = 'Error: ' + error.message;
        });
}

document.addEventListener('DOMContentLoaded', function() {
    testJavaScript();
});