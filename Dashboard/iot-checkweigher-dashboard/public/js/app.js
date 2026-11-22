// app.js - Helper script untuk modal hapus dan interaksi dashboard
document.addEventListener('DOMContentLoaded', function () {
    const deleteModal = document.getElementById('deleteModal');
    const deleteForm = document.getElementById('deleteForm');
    const deleteName = document.getElementById('deleteName');
    const btnCancel = document.getElementById('btnCancelDelete');

    if (deleteModal && deleteForm) {
        document.querySelectorAll('.btn-delete').forEach(button => {
            button.addEventListener('click', function () {
                const url = this.getAttribute('data-url');
                const name = this.getAttribute('data-name');
                deleteForm.setAttribute('action', url);
                deleteName.innerText = name;
                deleteModal.classList.remove('hidden');
            });
        });

        if (btnCancel) {
            btnCancel.addEventListener('click', function () {
                deleteModal.classList.add('hidden');
            });
        }

        deleteModal.addEventListener('click', function (e) {
            if (e.target === deleteModal) {
                deleteModal.classList.add('hidden');
            }
        });
    }
});

