const API_URL = '/api';

// Função para atualizar vagas
async function updateSlots() {
    try {
        const response = await fetch(`${API_URL}/slots`);
        const data = await response.json();
        
        document.getElementById('slot-a').innerText = `Vagas restantes: ${data.A}`;
        document.getElementById('slot-b').innerText = `Vagas restantes: ${data.B}`;
        document.getElementById('slot-c').innerText = `Vagas restantes: ${data.C}`;
        
        // Atualiza cabeçalho
        document.getElementById('slots-display').innerText = `A:${data.A} | B:${data.B} | C:${data.C}`;
        
    } catch (e) {
        console.error("Erro ao buscar vagas:", e);
    }
}

// Função de Inscrição
async function register(plan) {
    showToast("A processar inscrição...");
    
    try {
        // Envia pedido ao servidor C
        // Nota: O servidor C espera ?plan=X
        const response = await fetch(`${API_URL}/register?plan=${plan}`, {
            method: 'POST' 
        });
        
        const result = await response.json();
        
        if (response.status === 200) {
            showToast(`✅ ${result.message}`);
            updateSlots(); // Atualiza UI imediatamente
        } else {
            showToast(`❌ ${result.message}`);
        }
        
    } catch (e) {
        showToast("❌ Erro de conexão ao servidor.");
    }
}

function showToast(message) {
    const x = document.getElementById("toast");
    x.innerText = message;
    x.className = "toast show";
    setTimeout(function(){ x.className = x.className.replace("show", ""); }, 3000);
}

// Atualizar vagas a cada 2 segundos (Polling para simular tempo real)
setInterval(updateSlots, 2000);
updateSlots();