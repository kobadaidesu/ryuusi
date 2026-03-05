function scrollToContent() {
  window.scrollTo({ top: window.innerHeight, behavior: 'smooth' });
}

/* ---- Scroll reveal ---- */
const observer = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');
      observer.unobserve(entry.target);
    }
  });
}, { threshold: 0.12 });

document.querySelectorAll('.reveal, .reveal-fade').forEach(el => observer.observe(el));

/* ---- Lightbox ---- */
const lightbox    = document.getElementById('lightbox');
const lightboxImg = document.getElementById('lightbox-img');

function openLightbox(src, alt) {
  lightboxImg.src = src;
  lightboxImg.alt = alt;
  lightbox.classList.add('active');
  document.body.style.overflow = 'hidden';
}

function closeLightbox() {
  lightbox.classList.remove('active');
  document.body.style.overflow = '';
}

/* 画像クリック/タップで開く */
document.querySelectorAll('.param-img').forEach(img => {
  img.addEventListener('click', () => openLightbox(img.src, img.alt));
});

/* 背景クリックで閉じる */
lightbox.addEventListener('click', (e) => {
  if (e.target === lightbox || e.target.classList.contains('lightbox-bg')) {
    closeLightbox();
  }
});

/* Esc キーで閉じる */
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') closeLightbox();
});
