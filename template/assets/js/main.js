(function ($) {
  'use strict';

  // Activate Tooltips & Popovers
  $(function () {
    $('[data-toggle="tooltip"]').tooltip();
    $('[data-toggle="popover"]').popover();

    // Dismiss Popovers on next click
    $('.popover-dismiss').popover({
      trigger: 'focus'
    })
  });

  $(document).on('ready', function () {
    // Go to Top
    var go2TopShowHide = (function () {
      var $this = $('.js-go-to');

      $this.on("click", function (event) {
        event.preventDefault();
        $("html, body").animate({ scrollTop: 0 }, 600);
      });

      var go2TopOperation = function () {
        var CurrentWindowPosition = $(window).scrollTop();

        if (CurrentWindowPosition > 400) {
          $this.addClass("show");
        } else {
          $this.removeClass("show");
        }
      };

      go2TopOperation();

      $(window).scroll(function () {
        go2TopOperation();
      });
    }());
  });

  $(window).on('load', function () {

    // Page Nav
    var onePageScrolling = (function () {
      $('.js-scroll-nav a').on('click', function (event) {
        event.preventDefault();
        if ($('.duik-header').length) {
          $('html, body').animate({ scrollTop: ($('#' + this.href.split('#')[1]).offset().top - ($('.duik-header .navbar').height()) - 30) }, 600);
        } else {
          $('html, body').animate({ scrollTop: ($('#' + this.href.split('#')[1]).offset().top - 30) }, 600);
        }
      });
    }());

    var oneAnchorScrolling = (function () {
      $('.js-anchor-link').on('click', function (event) {
        event.preventDefault();
        if ($('.duik-header').length) {
          $('html, body').animate({ scrollTop: ($('#' + this.href.split('#')[1]).offset().top - ($('.duik-header .navbar').height()) - 30) }, 600);
        } else {
          $('html, body').animate({ scrollTop: ($('#' + this.href.split('#')[1]).offset().top - 30) }, 600);
        }
      });
    }());

    var prepareCopyToClipboard = (function () {
      $('.js-copy-to-clip').on('click', function (event) {
        event.preventDefault();

        // see https://stackoverflow.com/questions/400212/how-do-i-copy-to-the-clipboard-in-javascript
        // Copies a string to the clipboard. Must be called from within an
        // event handler such as click. May return false if it failed, but
        // this is not always possible. Browser support for Chrome 43+,
        // Firefox 42+, Safari 10+, Edge and Internet Explorer 10+.
        // Internet Explorer: The clipboard feature may be disabled by
        // an administrator. By default a prompt is shown the first
        // time the clipboard is used (per session).
        function copyToClipboard(text) {
          if (window.clipboardData && window.clipboardData.setData) {
            // Internet Explorer-specific code path to prevent textarea being shown while dialog is visible.
            return clipboardData.setData("Text", text);

          }
          else if (document.queryCommandSupported && document.queryCommandSupported("copy")) {
            var textarea = document.createElement("textarea");
            textarea.textContent = text;
            textarea.style.position = "fixed";  // Prevent scrolling to bottom of page in Microsoft Edge.
            document.body.appendChild(textarea);
            textarea.select();
            try {
              return document.execCommand("copy");  // Security exception may be thrown by some browsers.
            }
            catch (ex) {
              console.warn("Copy to clipboard failed.", ex);
              return false;
            }
            finally {
              document.body.removeChild(textarea);
            }
          }
        }

        copyToClipboard(this.getAttribute("copy"));

        // window.alert("hi!" + this.getAttribute("copy"));
        // TODO: fixme
        // $(this).tooltip({ content: "copied" });
        // $(this).tooltip("open");
      });
    }());
  });
})(jQuery);