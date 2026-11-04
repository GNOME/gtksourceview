<%@ page contentType="text/html; charset=UTF-8" session="true" %>
<%@ page import="java.util.List" %>
<%@ taglib prefix="c" uri="http://java.sun.com/jsp/jstl/core" %>

<%-- A JSP comment never reaches the client, unlike an <!-- HTML --> one. --%>
<%!
    private static final String GREETING = "Welcome back";

    private int visitCount (Object attribute)
    {
        return attribute instanceof Integer ? (Integer) attribute : 0;
    }
%>

<!doctype html>
<html>
    <head>
        <title>${pageContext.request.contextPath}</title>
        <script>
            const user = '${sessionScope.user.name}';
            document.addEventListener('load', function (event) {
                document.body.classList.add('has-js');
            });
        </script>
    </head>

    <body>
        <%
            String name = (String) session.getAttribute ("name");
            int seen = visitCount (session.getAttribute ("visits"));
            String closer = "%>"; /* must not end the scriptlet */
            if (seen > 0 && !GREETING.isEmpty ())
            {
                out.print (closer.length ());
            }
        %>

        <c:choose>
            <c:when test="${not empty sessionScope.user and requestScope.total ge 1}">
                <h1><%= GREETING %>, <c:out value="${sessionScope.user.name}"/></h1>
            </c:when>
            <c:otherwise>
                <h1>Hello!</h1>
                <p>Resolved name: <%= name != null ? name : "stranger" %></p>
            </c:otherwise>
        </c:choose>

        <c:forEach var="item" items="${requestScope.cart}" varStatus="s">
            <p>${s.index + 1}. ${item.label} &mdash; ${item.price * 1.075}</p>
        </c:forEach>

        <p>Deferred: #{bean.total ne null ? bean.total : 0}</p>
        <p>Escaped opener: <\% is template text, not a scriptlet.</p>

        <jsp:include page="footer.jsp">
            <jsp:param name="year" value="${initParam.copyrightYear}"/>
        </jsp:include>
    </body>
</html>
